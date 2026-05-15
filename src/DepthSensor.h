#ifndef DEPTH_SENSOR_H
#define DEPTH_SENSOR_H

#include <Arduino.h>
#include <Wire.h>

/*
 * 深度传感器模块 (软件I2C / 硬件I2C)
 *
 * 移植自 STM32 工程的 depth_sensor.c / depth_sensor.h
 * STM32 原版使用 GPIO 模拟 I2C (PB6=SCL, PB7=SDA)
 * ESP32 版本使用硬件 I2C，默认引脚: SCL=GPIO8, SDA=GPIO9
 *
 * 注意: STM32原版代码中 Depth_Sensor_Read() 返回的是模拟数据，
 *       这里保留了通用的I2C读取框架，并提供了MS5837深度传感器的
 *       读取实现。如果你的深度传感器型号不同，请修改读取逻辑。
 *
 * 支持的传感器型号:
 *   - MS5837 (默认，常见水深压力传感器)
 *   - 其他I2C深度/压力传感器（需自行实现读取逻辑）
 */

// I2C引脚定义 (ESP32 Nano)
#define DEPTH_I2C_SDA  9   // 对应STM32的PB7
#define DEPTH_I2C_SCL  8   // 对应STM32的PB6

// MS5837 I2C地址 (标准地址)
#define MS5837_ADDR    0x76
// 备用地址 (部分MS5837模块)
#define MS5837_ADDR_ALT 0x77

// MS5837 命令定义
#define MS5837_RESET       0x1E
#define MS5837_ADC_READ    0x00
#define MS5837_PROM_READ   0xA0

// OSR (过采样率) 设置
#define MS5837_CONVERT_D1_OSR256  0x40
#define MS5837_CONVERT_D1_OSR512  0x42
#define MS5837_CONVERT_D1_OSR1024 0x44
#define MS5837_CONVERT_D1_OSR2048 0x46
#define MS5837_CONVERT_D1_OSR4096 0x48
#define MS5837_CONVERT_D2_OSR256  0x50
#define MS5837_CONVERT_D2_OSR512  0x52
#define MS5837_CONVERT_D2_OSR1024 0x54
#define MS5837_CONVERT_D2_OSR2048 0x56
#define MS5837_CONVERT_D2_OSR4096 0x58

// 深度传感器类型
enum DepthSensorType {
    DEPTH_MS5837,       // MS5837 压力/深度传感器
    DEPTH_GENERIC_I2C   // 通用I2C深度传感器（需要自定义读取逻辑）
};

class DepthSensor {
public:
    DepthSensor();

    /**
     * 初始化深度传感器
     * @param sensorType 传感器类型，默认MS5837
     * @param sda I2C数据引脚，默认DEPTH_I2C_SDA
     * @param scl I2C时钟引脚，默认DEPTH_I2C_SCL
     * @return true=初始化成功, false=初始化失败
     */
    bool begin(DepthSensorType sensorType = DEPTH_MS5837,
               uint8_t sda = DEPTH_I2C_SDA,
               uint8_t scl = DEPTH_I2C_SCL);

    /**
     * 读取深度值（米）
     * @return 深度值（单位：米），读取失败返回-999.0
     */
    float readDepth();

    /**
     * 读取压力值（mbar）
     * @return 压力值（单位：mbar），读取失败返回-999.0
     */
    float readPressure();

    /**
     * 读取温度值（℃），来自深度传感器内置温度
     * 注意：这是深度传感器内部的温度，与DS18B20水温不同
     * @return 温度值（单位：℃），读取失败返回-999.0
     */
    float readTemperature();

    /**
     * 检查传感器是否在线
     * @return true=在线, false=离线
     */
    bool isConnected();

    /**
     * 设置流体密度（用于深度计算）
     * @param density 流体密度（kg/m³），淡水默认997，海水默认1025
     */
    void setFluidDensity(float density);

    /**
     * 获取最后一次读取的错误码
     * @return 0=无错误, 其他=错误码
     */
    int getLastError();

private:
    DepthSensorType _sensorType;
    uint8_t _sda;
    uint8_t _scl;
    float _fluidDensity;
    int _lastError;
    bool _initialized;

    // MS5837 校准系数
    uint16_t _ms5837_prom[7];  // C1-C6
    bool _ms5837_detected;

    // MS5837 内部方法
    bool ms5837_init();
    bool ms5837_readProm();
    uint32_t ms5837_readADC(uint8_t cmd);
    void ms5837_cmd(uint8_t cmd);
    float ms5837_calculate(float &pressure, float &temperature, float &depth);

    // 通用I2C深度传感器读取
    float genericReadDepth();
};

#endif // DEPTH_SENSOR_H
