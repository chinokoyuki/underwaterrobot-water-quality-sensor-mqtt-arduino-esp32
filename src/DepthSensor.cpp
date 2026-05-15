#include "DepthSensor.h"

/*
 * 深度传感器模块 - Arduino ESP32 实现
 *
 * 移植说明:
 *   STM32原版使用GPIO模拟I2C（软件I2C），ESP32拥有硬件I2C控制器，
 *   因此改用Wire库（硬件I2C）实现，更稳定且代码更简洁。
 *
 *   STM32原版引脚映射:
 *     PB6 (I2C_SCL) -> ESP32 GPIO8 (DEPTH_I2C_SCL)
 *     PB7 (I2C_SDA) -> ESP32 GPIO9 (DEPTH_I2C_SDA)
 *
 *   STM32原版 depth_sensor.c 中的 Depth_Sensor_Read() 返回模拟数据 1.23f，
 *   这里实现了MS5837的真实读取逻辑。
 */

// 构造函数
DepthSensor::DepthSensor() {
    _sensorType = DEPTH_MS5837;
    _sda = DEPTH_I2C_SDA;
    _scl = DEPTH_I2C_SCL;
    _fluidDensity = 997.0f;  // 默认淡水密度
    _lastError = 0;
    _initialized = false;
    _ms5837_detected = false;
    for (int i = 0; i < 7; i++) {
        _ms5837_prom[i] = 0;
    }
}

// 初始化
bool DepthSensor::begin(DepthSensorType sensorType, uint8_t sda, uint8_t scl) {
    _sensorType = sensorType;
    _sda = sda;
    _scl = scl;
    _initialized = false;
    _ms5837_detected = false;

    // 初始化I2C
    Wire.begin(_sda, _scl);
    Wire.setClock(100000);  // 100kHz，与STM32软件I2C速度接近

    delay(100);  // 等待传感器上电稳定

    if (_sensorType == DEPTH_MS5837) {
        _initialized = ms5837_init();
    } else {
        // 通用I2C传感器，仅检测I2C总线是否可用
        Wire.beginTransmission(MS5837_ADDR);
        _initialized = (Wire.endTransmission() == 0);
    }

    if (!_initialized) {
        _lastError = 1;  // 传感器未响应
        Serial.println("[DepthSensor] 初始化失败，传感器未响应");
    } else {
        Serial.println("[DepthSensor] 初始化成功");
    }

    return _initialized;
}

// ========== MS5837 实现 ==========

bool DepthSensor::ms5837_init() {
    // 尝试标准地址
    ms5837_cmd(MS5837_RESET);
    delay(10);

    if (!ms5837_readProm()) {
        // 尝试备用地址
        Wire.beginTransmission(MS5837_ADDR_ALT);
        if (Wire.endTransmission() == 0) {
            // 备用地址可用，但这里简化处理，仅使用标准地址
            Serial.println("[DepthSensor] 检测到备用地址设备");
        }
        // 仍然尝试读取PROM
        if (!ms5837_readProm()) {
            Serial.println("[DepthSensor] MS5837 PROM读取失败");
            return false;
        }
    }

    _ms5837_detected = true;
    return true;
}

bool DepthSensor::ms5837_readProm() {
    // 读取7个校准系数 (C1-C6 + CRC)
    for (uint8_t i = 0; i < 7; i++) {
        Wire.beginTransmission(MS5837_ADDR);
        Wire.write(MS5837_PROM_READ + i * 2);
        if (Wire.endTransmission() != 0) return false;

        Wire.requestFrom(MS5837_ADDR, (uint8_t)2);
        if (Wire.available() < 2) return false;

        _ms5837_prom[i] = (Wire.read() << 8) | Wire.read();
    }

    // 验证CRC (简化：检查C1-C6是否全为0)
    bool allZero = true;
    for (uint8_t i = 0; i < 6; i++) {
        if (_ms5837_prom[i] != 0) {
            allZero = false;
            break;
        }
    }
    if (allZero) {
        Serial.println("[DepthSensor] PROM数据全为0，可能传感器未连接");
        return false;
    }

    // 打印校准系数（调试用）
    Serial.println("[DepthSensor] MS5837 校准系数:");
    for (uint8_t i = 0; i < 7; i++) {
        Serial.print("  C");
        Serial.print(i + 1);
        Serial.print(" = ");
        Serial.println(_ms5837_prom[i]);
    }

    return true;
}

void DepthSensor::ms5837_cmd(uint8_t cmd) {
    Wire.beginTransmission(MS5837_ADDR);
    Wire.write(cmd);
    Wire.endTransmission();
}

uint32_t DepthSensor::ms5837_readADC(uint8_t cmd) {
    ms5837_cmd(cmd);

    // 根据OSR等待转换完成
    // OSR256: 0.6ms, OSR512: 1.2ms, OSR1024: 2.3ms,
    // OSR2048: 4.6ms, OSR4096: 9.2ms
    // 这里统一等待10ms，足够覆盖最高OSR
    delay(10);

    ms5837_cmd(MS5837_ADC_READ);
    Wire.requestFrom(MS5837_ADDR, (uint8_t)3);
    if (Wire.available() < 3) return 0;

    uint32_t val = Wire.read();
    val = (val << 8) | Wire.read();
    val = (val << 8) | Wire.read();
    return val;
}

float DepthSensor::ms5837_calculate(float &pressure, float &temperature, float &depth) {
    // 读取D1 (压力数字值) 和 D2 (温度数字值)
    uint32_t D1 = ms5837_readADC(MS5837_CONVERT_D1_OSR2048);
    uint32_t D2 = ms5837_readADC(MS5837_CONVERT_D2_OSR2048);

    if (D1 == 0 || D2 == 0) {
        _lastError = 2;  // ADC读取失败
        return false;
    }

    // 校准系数
    uint16_t C1 = _ms5837_prom[0];
    uint16_t C2 = _ms5837_prom[1];
    uint16_t C3 = _ms5837_prom[2];
    uint16_t C4 = _ms5837_prom[3];
    uint16_t C5 = _ms5837_prom[4];
    uint16_t C6 = _ms5837_prom[5];

    // 计算温差
    int32_t dT = (int32_t)D2 - ((int32_t)C5 << 8);

    // 计算实际温度 (单位: 0.01℃)
    int32_t TEMP = 2000 + ((int64_t)dT * C6 >> 23);

    // 计算偏移温度补偿后的压力
    int64_t OFF  = ((int64_t)C2 << 16) + ((int64_t)C4 * dT >> 7);
    int64_t SENS = ((int64_t)C1 << 15) + ((int64_t)C3 * dT >> 8);

    // 二阶温度补偿
    int64_t OFF2  = 0;
    int64_t SENS2 = 0;
    int32_t TEMP2 = 0;

    if (TEMP < 2000) {
        // 低温补偿
        int32_t Ti = (int64_t)(11 * dT * dT) >> 35;
        OFF2  = (int64_t)(31 * (TEMP - 2000) * (TEMP - 2000)) >> 3;
        SENS2 = (int64_t)(63 * (TEMP - 2000) * (TEMP - 2000)) >> 5;
        TEMP2 = Ti;
    }

    TEMP = TEMP - TEMP2;
    OFF  = OFF  - OFF2;
    SENS = SENS - SENS2;

    // 计算压力 (单位: 0.01 mbar)
    int32_t P = ((int64_t)D1 * SENS >> 21) - OFF;
    P = P >> 15;

    // 转换为标准单位
    temperature = (float)TEMP / 100.0f;              // ℃
    pressure    = (float)P / 100.0f;                  // mbar
    float absPressure = (float)P / 100000.0f;         // bar

    // 计算深度（米）
    // 深度 = (绝对压力 - 大气压) / (流体密度 × 重力加速度)
    // 大气压 ≈ 1013.25 mbar ≈ 1.01325 bar
    depth = (absPressure - 1.01325f) * 100000.0f / (_fluidDensity * 9.80665f);

    // 如果计算结果为负（水面之上），置零
    if (depth < 0) depth = 0;

    return true;
}

// ========== 公共接口实现 ==========

float DepthSensor::readDepth() {
    if (!_initialized) {
        _lastError = 3;  // 未初始化
        return -999.0f;
    }

    if (_sensorType == DEPTH_MS5837 && _ms5837_detected) {
        float pressure, temperature, depth;
        if (ms5837_calculate(pressure, temperature, depth)) {
            _lastError = 0;
            return depth;
        }
        return -999.0f;
    }

    // 通用I2C深度传感器
    return genericReadDepth();
}

float DepthSensor::readPressure() {
    if (!_initialized) return -999.0f;

    if (_sensorType == DEPTH_MS5837 && _ms5837_detected) {
        float pressure, temperature, depth;
        if (ms5837_calculate(pressure, temperature, depth)) {
            _lastError = 0;
            return pressure;
        }
        return -999.0f;
    }

    return -999.0f;
}

float DepthSensor::readTemperature() {
    if (!_initialized) return -999.0f;

    if (_sensorType == DEPTH_MS5837 && _ms5837_detected) {
        float pressure, temperature, depth;
        if (ms5837_calculate(pressure, temperature, depth)) {
            _lastError = 0;
            return temperature;
        }
        return -999.0f;
    }

    return -999.0f;
}

bool DepthSensor::isConnected() {
    Wire.beginTransmission(MS5837_ADDR);
    return (Wire.endTransmission() == 0);
}

void DepthSensor::setFluidDensity(float density) {
    _fluidDensity = density;
}

int DepthSensor::getLastError() {
    return _lastError;
}

// ========== 通用I2C深度传感器 ==========

float DepthSensor::genericReadDepth() {
    /*
     * 通用I2C深度传感器读取模板
     *
     * 如果你使用的深度传感器不是MS5837，请在此实现你的读取逻辑。
     * 参考STM32原版代码中的通用流程：
     *
     *   I2C_Start();
     *   I2C_Send_Byte(0xEE);  // 写地址
     *   I2C_Send_Byte(0x00);  // 寄存器
     *   I2C_Start();
     *   I2C_Send_Byte(0xEF);  // 读地址
     *   // ... 读取逻辑 ...
     *   I2C_Stop();
     *
     * Arduino Wire 等价写法:
     *   Wire.beginTransmission(0x77);  // 设备地址(7位)
     *   Wire.write(0x00);             // 寄存器地址
     *   Wire.endTransmission();
     *   Wire.requestFrom(0x77, 2);    // 读取2字节
     *   uint16_t raw = (Wire.read() << 8) | Wire.read();
     */

    // 默认返回-1表示未实现
    _lastError = 4;  // 通用传感器未实现
    return -1.0f;
}
