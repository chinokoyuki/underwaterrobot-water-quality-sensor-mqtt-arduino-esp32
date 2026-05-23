#ifndef WATER_QUALITY_SENSOR_H
#define WATER_QUALITY_SENSOR_H

#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Preferences.h>
#include "DepthSensor.h"

// 引脚定义
#define PH_PIN A0                // pH传感器模拟输入引脚
#define TDS_PIN A1               // TDS传感器模拟输入引脚
#define TURBIDITY_PIN A2         // 浊度传感器模拟输入引脚
#define DO_PIN A3              // 溶解氧传感器模拟输入引脚（原氨氮传感器引脚）
// 水温传感器（DS18B20）引脚（PA4→A4，单总线）
#define TEMP_SENSOR_PIN A4       // DS18B20单总线引脚
// 传感器使能引脚（PB0→GPIO2，数字输出）
#define SENSOR_ENABLE_PIN 2      // 传感器电源使能引脚
// 报警引脚（PB1→GPIO3，数字输出，驱动蜂鸣器/LED）
#define ALARM_PIN 3              // 水质超标报警引脚

// 校准常量
#define PH_REF_VOLTAGE 3.3f      // Nano ESP32参考电压3.3V
#define PH_OFFSET 0.0f           // pH校准偏移量（根据实际传感器调整）
#define PH_SLOPE (-5.0f/3.0f)    // pH传感器斜率（通常为-5.0/3.0）
#define TDS_REF_VOLTAGE 3.3f     // TDS参考电压
#define TDS_TEMPERATURE 25.0f    // 水温
#define DO_REF_VOLTAGE 3.3f    // 溶解氧参考电压
#define DO_MAX_VALUE 20.0f     // 溶解氧最大测量值（mg/L）
#define DO_COEFFICIENT 1.0f    // 溶解氧校准系数（默认1.0）
// 水质超标阈值
#define PH_MIN_NORMAL 6.0f       // pH最低正常值
#define PH_MAX_NORMAL 8.0f       // pH最高正常值
#define TDS_MAX_NORMAL 500.0f    // TDS最大正常值（ppm）
#define DO_MIN_NORMAL 2.0f       // 溶解氧最低正常值（mg/L），低于此值视为水质异常
// 深度超标阈值（单位：米）
#define DEPTH_MAX_NORMAL 10.0f   // 最大正常深度（根据实际场景调整）

// ADC衰减配置（优化小电压采集精度，Nano ESP32支持4种衰减）
#define ADC_ATTENUATION ADC_11db // 11dB衰减，支持0-3.9V采集（适配3.3V传感器）

extern OneWire oneWire;
extern DallasTemperature ds18b20;
extern Preferences pref;

class WaterQualitySensor {
public:
    // struct
    WaterQualitySensor();
    // 初始化传感器
    void begin();
    // 读PH
    float readPH();
    // 读TDS
    float readTDS();
    // 读浊度
    double readTurbidity();
    // 读溶解氧值
    float readDissolvedOxygen();
    // 实时水温读取函数
    float readTemperature();
    // 深度读取函数（新增，移植自STM32二版工程）
    float readDepth();
    // 深度传感器压力读取
    float readDepthPressure();
    // 深度传感器内部温度读取
    float readDepthTemperature();
    // 深度传感器连接状态
    bool isDepthSensorConnected();
    // 设置流体密度（淡水/海水）
    void setFluidDensity(float density);
    // 传感器使能/禁用函数
    void enableSensor();
    void disableSensor();
    // 水质超标判断函数（含深度检测）
    bool isWaterQualityNormal(float ph, float tds, float dissolvedOxygen, float depth = -1.0f);
    // 报警触发/关闭函数
    void triggerAlarm();
    void stopAlarm();
    // 校准参数存储/读取函数
    void saveCalibrationParams() ;
    void loadCalibrationParams();

private:
    // 读取模拟引脚并计算平均电压（去噪）
    float readAverageVoltage(uint8_t pin, uint8_t sampleCount = 10);
    // 深度传感器对象
    DepthSensor _depthSensor;
    // 校参（掉电）
    float _phOffset = 0.0f;
    float _tdsCoefficient = 133.42f;
    float _doCoefficient = 1.0f;  // 溶解氧校准系数
};

#endif