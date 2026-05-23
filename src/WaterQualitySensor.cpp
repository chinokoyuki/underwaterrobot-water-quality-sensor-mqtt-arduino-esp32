#include "WaterQualitySensor.h"

// 构造函数
WaterQualitySensor::WaterQualitySensor() {
    _phOffset = 0.0f;
    _tdsCoefficient = 133.42f;
    _doCoefficient = 1.0f;
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
    pinMode(DO_PIN, INPUT);
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

// 读取pH值（公式移植自 Bionic manta ray 工程）
float WaterQualitySensor::readPH() {
    float phVoltage = readAverageVoltage(PH_PIN);
    // pH = -12.384 * V + 40.93（V 单位：伏特）
    float phValue = -12.384f * phVoltage + 40.93f;
    // 限制pH值范围（0-14）
    return constrain(phValue, 0.0f, 14.0f);
}

// 读取TDS值（三次多项式公式，移植自 Bionic manta ray 工程）
float WaterQualitySensor::readTDS() {
    float tdsVoltage = readAverageVoltage(TDS_PIN);
    // 将电压转换为毫伏（mV）
    float tdsVoltageMv = tdsVoltage * 1000.0f;
    // 三次多项式公式：TDS = 66.71*v^3/1e9 - 127.93*v^2/1e6 + 428.7*v/1e3
    float v = tdsVoltageMv;
    float tdsValue = 66.71f * v * v * v / 1000000000.0f
                   - 127.93f * v * v / 1000000.0f
                   + 428.7f * v / 1000.0f;
    return constrain(tdsValue, 0.0f, 2000.0f); // 限制TDS范围（0-2000ppm）
}

// 读取浊度值（温度补偿公式，移植自 Bionic manta ray 工程）
double WaterQualitySensor::readTurbidity() {
    float turbVoltage = readAverageVoltage(TURBIDITY_PIN);
    // 电压转毫伏
    float turbVoltageMv = turbVoltage * 1000.0f;
    // 获取实时水温进行温度补偿
    ds18b20.requestTemperatures();
    float waterTemp = ds18b20.getTempCByIndex(0);
    if (waterTemp < -127.0f) waterTemp = 25.0f; // 传感器异常时使用默认温度
    // 温度补偿后的电压(mV): voltage_mV - (-0.0192 * (temp - 25))
    float compensatedVoltageMv = turbVoltageMv - (-0.0192f * (waterTemp - 25.0f));
    // 浊度 = -865.68 * (补偿后电压V) + 2047.19
    double result = -865.68 * (compensatedVoltageMv / 1000.0) + 2047.19;
    return result;
}

// 读取溶解氧值（查表+温度补偿公式，移植自 Bionic manta ray 工程）
// DoList: 不同温度(0~40℃)下的饱和溶解氧电压值(mV)
static const int DO_LIST[] = {
    1462, 1422, 1383, 1346, 1311, 1277, 1245, 1214, 1184, 1156,
    1129, 1103, 1078, 1054, 1031, 1008, 987, 966, 947, 928,
    909, 891, 874, 858, 842, 826, 811, 797, 783, 769,
    756, 743, 730, 718, 707, 695, 684, 673, 663, 653, 643
};
#define DO_LIST_SIZE 41

float WaterQualitySensor::readDissolvedOxygen() {
    // 获取实时水温
    ds18b20.requestTemperatures();
    float temp = ds18b20.getTempCByIndex(0);
    if (temp < -127.0f) temp = 25.0f; // 传感器异常时使用默认温度
    // 读取溶解氧传感器平均电压（mV）
    float doVoltage = readAverageVoltage(DO_PIN);
    float doVoltageMv = doVoltage * 1000.0f;
    // 查表获取当前温度下的饱和溶解氧电压值
    int tempIndex = (int)(temp + 0.5f); // 四舍五入
    if (tempIndex < 0) tempIndex = 0;
    if (tempIndex >= DO_LIST_SIZE) tempIndex = DO_LIST_SIZE - 1;
    float satVoltage = (float)DO_LIST[tempIndex];
    // 溶解氧浓度计算公式（移植自 dataAccess.js）：
    // DO = ((voltage_mV - 55) * 769 / 805 - (842 - DoList[round(temp)])) / 100
    float doValue = ((doVoltageMv - 55.0f) * 769.0f / 805.0f
                    - (842.0f - satVoltage)) / 100.0f;
    doValue *= _doCoefficient; // 应用校准系数
    return constrain(doValue, 0.0f, DO_MAX_VALUE);
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
bool WaterQualitySensor::isWaterQualityNormal(float ph, float tds, float dissolvedOxygen, float depth) {
    if (ph < PH_MIN_NORMAL || ph > PH_MAX_NORMAL) return false;
    if (tds > TDS_MAX_NORMAL) return false;
    if (dissolvedOxygen < DO_MIN_NORMAL) return false; // 溶解氧过低
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
    pref.putFloat("doCoeff", _doCoefficient);
    pref.end(); // 关闭命名空间
}

void WaterQualitySensor::loadCalibrationParams() {
    pref.begin("water-quality", false); // 只读模式
    // 读取参数（无历史数据则用默认值）
    _phOffset = pref.getFloat("phOffset", 0.0f);
    _tdsCoefficient = pref.getFloat("tdsCoeff", 133.42f);
    _doCoefficient = pref.getFloat("doCoeff", 1.0f);
    pref.end();
}