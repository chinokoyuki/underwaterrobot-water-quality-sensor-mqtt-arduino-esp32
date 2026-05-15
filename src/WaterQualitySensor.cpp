#include "WaterQualitySensor.h"

// 构造函数
WaterQualitySensor::WaterQualitySensor() {
    _phOffset = 0.0f;
    _tdsCoefficient = 133.42f;
    _ammoniaCoefficient = 1.0f;
}

OneWire oneWire(TEMP_SENSOR_PIN);
DallasTemperature ds18b20(&oneWire);
Preferences pref;

// 初始化
void WaterQualitySensor::begin() {
    // 配置ADC衰减系数（优化精度）
    analogSetAttenuation(ADC_ATTENUATION);
    // 初始化传感器使能引脚
    pinMode(SENSOR_ENABLE_PIN, OUTPUT);
    enableSensor(); // 默认开启传感器供电
    // 初始化报警引脚
    pinMode(ALARM_PIN, OUTPUT);
    stopAlarm(); // 默认关闭报警
    pinMode(PH_PIN, INPUT);
    pinMode(TDS_PIN, INPUT);
    pinMode(TURBIDITY_PIN, INPUT);
    pinMode(AMMONIA_NITROGEN_PIN, INPUT);
    // 初始化水温传感器
    ds18b20.begin();
    // 初始化深度传感器（新增，移植自STM32二版工程）
    if (_depthSensor.begin()) {
        Serial.println("[WaterSensor] 深度传感器初始化成功");
    } else {
        Serial.println("[WaterSensor] 深度传感器初始化失败，将跳过深度数据");
    }
    // 初始化掉电存储，加载历史校准参数
    pref.begin("water-quality", false); // 打开命名空间，只读模式
    loadCalibrationParams();
}

// 读取平均电压（去噪，采样10次）
float WaterQualitySensor::readAverageVoltage(uint8_t pin, uint8_t sampleCount) {
    float sum = 0.0f;
    for (uint8_t i = 0; i < sampleCount; i++) {
        sum += analogRead(pin);  // Nano ESP32 ADC分辨率默认12位
        delay(10);               // 间隔
    }
    float avgAdc = sum / sampleCount;
    // 转换为电压值：电压 = (ADC值 / 4095) * 参考电压
    return (avgAdc / 4095.0f) * PH_REF_VOLTAGE;
}

// 读取pH值
float WaterQualitySensor::readPH() {
    float phVoltage = readAverageVoltage(PH_PIN);
    // pH计算公式：pH = 7 + (参考电压/2 - 传感器电压) / 斜率 + 偏移量
    float phValue = 7.0f + (PH_REF_VOLTAGE/2 - phVoltage) * PH_SLOPE + PH_OFFSET;
    // 限制pH值范围（0-14）
    return constrain(phValue, 0.0f, 14.0f);
}

// 读取TDS值
float WaterQualitySensor::readTDS() {
    float tdsVoltage = readAverageVoltage(TDS_PIN);
    // 补偿水温对TDS的影响
    float temperatureCompensation = 1.0f + 0.02f * (TDS_TEMPERATURE - 25.0f);
    // 计算TDS值（ppm）：TDS = (电压值 / 参考电压) * 133.42 * 补偿系数
    float tdsValue = (tdsVoltage / TDS_REF_VOLTAGE) * 133.42f * temperatureCompensation;
    return constrain(tdsValue, 0.0f, 2000.0f); // 限制TDS范围（0-2000ppm）
}

// 读取浊度值
int WaterQualitySensor::readTurbidity() {
    return analogRead(TURBIDITY_PIN); // 直接返回ADC原始值（0-4095）
}

// 读取氨氮值
float WaterQualitySensor::readAmmoniaNitrogen() {
    // 读取平均电压
    float ammoniaVoltage = readAverageVoltage(AMMONIA_NITROGEN_PIN);
    float ammoniaValue = (ammoniaVoltage / AMMONIA_REF_VOLTAGE) * AMMONIA_MAX_VALUE * _ammoniaCoefficient;
    // 限制浓度范围，避免异常值
    return constrain(ammoniaValue, 0.0f, AMMONIA_MAX_VALUE);
}

// 读取实时水温
float WaterQualitySensor::readTemperature() {
    ds18b20.requestTemperatures();
    float temp = ds18b20.getTempCByIndex(0);
    if (temp < -127.0f) {
        return -1.0f;
    }
    return constrain(temp, 0.0f, 100.0f);
}

// 读取深度值（米）—— 新增，移植自STM32二版工程
float WaterQualitySensor::readDepth() {
    return _depthSensor.readDepth();
}

// 读取深度传感器压力值（mbar）
float WaterQualitySensor::readDepthPressure() {
    return _depthSensor.readPressure();
}

// 读取深度传感器内部温度（℃）
float WaterQualitySensor::readDepthTemperature() {
    return _depthSensor.readTemperature();
}

// 深度传感器连接状态
bool WaterQualitySensor::isDepthSensorConnected() {
    return _depthSensor.isConnected();
}

// 设置流体密度
void WaterQualitySensor::setFluidDensity(float density) {
    _depthSensor.setFluidDensity(density);
}

// 传感器使能/禁用
void WaterQualitySensor::enableSensor() {
    digitalWrite(SENSOR_ENABLE_PIN, HIGH); // 高电平使能
    delay(500);
}

void WaterQualitySensor::disableSensor() {
    digitalWrite(SENSOR_ENABLE_PIN, LOW); // 低电平禁用
}


// 水质超标判断（含深度检测）
bool WaterQualitySensor::isWaterQualityNormal(float ph, float tds, float ammonia, float depth) {
    if (ph < PH_MIN_NORMAL || ph > PH_MAX_NORMAL) return false;
    if (tds > TDS_MAX_NORMAL) return false;
    if (ammonia > AMMONIA_MAX_NORMAL) return false;
    // 深度检测（仅在深度传感器有效时检测）
    if (depth >= 0 && depth > DEPTH_MAX_NORMAL) return false;
    return true;
}

// 报警触发/关闭
void WaterQualitySensor::triggerAlarm() {
    digitalWrite(ALARM_PIN, HIGH); // 高电平触发
}

void WaterQualitySensor::stopAlarm() {
    digitalWrite(ALARM_PIN, LOW); // 低电平关闭
}

// 校准参数掉电存储（ESP32 Flash）
void WaterQualitySensor::saveCalibrationParams() {
    pref.begin("water-quality", true);
    pref.putFloat("phOffset", _phOffset);
    pref.putFloat("tdsCoeff", _tdsCoefficient);
    pref.putFloat("ammoniaCoeff", _ammoniaCoefficient);
    pref.end(); // 关闭命名空间
}

void WaterQualitySensor::loadCalibrationParams() {
    pref.begin("water-quality", false); // 只读模式
    // 读取参数（无历史数据则用默认值）
    _phOffset = pref.getFloat("phOffset", 0.0f);
    _tdsCoefficient = pref.getFloat("tdsCoeff", 133.42f);
    _ammoniaCoefficient = pref.getFloat("ammoniaCoeff", 1.0f);
    pref.end();
}