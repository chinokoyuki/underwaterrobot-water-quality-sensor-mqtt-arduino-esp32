# Arduino ESP32 水质传感器项目 - 长期记忆

## 项目概况
- 平台：Arduino Nano ESP32 (ESP32-S3)
- 框架：Arduino + PlatformIO
- 项目路径：D:\Project\Arudino_ESP32

## 传感器配置
| 传感器 | 引脚 | 接口 |
|---|---|---|
| pH | A0 | 模拟 |
| TDS | A1 | 模拟 |
| 浊度 | A2 | 模拟 |
| 氨氮 | A3 | 模拟 |
| DS18B20水温 | A4 | 单总线(OneWire) |
| MS5837深度 | GPIO8(SCL)/GPIO9(SDA) | I2C |
| 传感器使能 | GPIO2 | 数字输出 |
| 报警蜂鸣器 | GPIO3 | 数字输出 |

## 云端配置
- MQTT平台：华为云IoT
- WiFi/MQTT凭据在main.cpp中配置

## 代码结构
- src/main.cpp - 主程序（WiFi/MQTT/主循环）
- src/WaterQualitySensor.h/cpp - 水质传感器类（pH/TDS/浊度/氨氮/水温/深度）
- src/DepthSensor.h/cpp - 深度传感器类（MS5837 I2C驱动）

## STM32源工程
- 路径：C:\Users\arisc\Desktop\水质传感器二版
- 平台：STM32F103C8T6, StdPeriph库
- 已完成深度传感器和DS18B20的移植（2026-05-15）
