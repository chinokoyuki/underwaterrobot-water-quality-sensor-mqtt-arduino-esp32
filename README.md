# 🌊 水下机器人水质传感器系统

> **Arduino Nano ESP32 平台 · 多参数水质监测 · 华为云 MQTT 上报**

基于 **Arduino Nano ESP32** 开发的水质多参数采集与云端上报系统，搭载 **pH / TDS / 浊度 / 氨氮 / DS18B20 水温 / MS5837 深度** 六合一传感器阵列，支持超标报警与华为云 IoT 平台实时数据上报。

---

## 🧱 技术栈

| 层面 | 技术 | 说明 |
|------|------|------|
| **主控芯片** | ESP32-S3 (Xtensa LX7 双核 240MHz) | 搭载在 Arduino Nano ESP32 开发板上 |
| **开发框架** | [PlatformIO](https://platformio.org/) + [Arduino](https://www.arduino.cc/) | 跨平台嵌入式构建系统 |
| **编程语言** | C++11 (Arduino 框架风格) | 面向对象封装传感器类 |
| **通信协议** | **WiFi** (802.11 b/g/n) + **MQTT** (PubSubClient) | 无线连接华为云 IoT 平台 |
| **云平台** | [华为云 IoT 设备接入](https://www.huaweicloud.com/product/iothub.html) | 设备影子 + 属性上报 |

### 📦 关键依赖库

| 库名 | 用途 | 版本 |
|------|------|------|
| [OneWire](https://github.com/PaulStoffregen/OneWire) | DS18B20 单总线通信 | ^2.3.8 |
| [DallasTemperature](https://github.com/milesburton/Arduino-Temperature-Control-Library) | DS18B20 温度读取 | ^4.0.5 |
| [PubSubClient](https://github.com/knolleary/pubsubclient) | MQTT 客户端协议实现 | ^2.8 |
| [ArduinoJson](https://arduinojson.org/) | JSON 数据序列化（预留） | — |
| [FastLED](https://github.com/FastLED/FastLED) | LED 状态指示（预留） | — |
| [BearSSL](https://github.com/spotify/bearssl) | SSL/TLS 加密通信（预留） | — |

### 🧩 传感器列表

| 传感器 | 测量参数 | 接口类型 | 引脚 |
|--------|---------|---------|------|
| pH 电极模块 | 酸碱度 (0-14) | 模拟输入 | A0 |
| TDS 笔模块 | 溶解性固体 (ppm) | 模拟输入 | A1 |
| 浊度传感器 | 水体浊度 (ADC) | 模拟输入 | A2 |
| 氨氮传感器 | 氨氮浓度 (mg/L) | 模拟输入 | A3 |
| DS18B20 | 水温 (℃) | 单总线 (OneWire) | A4 |
| **MS5837** | 深度 (m) / 压力 (mbar) | **I2C** | **D8(SCL) / D9(SDA)** |

### 🏗️ 软件架构

```
main.cpp
├── WaterQualitySensor          # 水质传感器主控类
│   ├── readPH()                # pH 读取
│   ├── readTDS()               # TDS 读取
│   ├── readTurbidity()         # 浊度读取
│   ├── readAmmoniaNitrogen()   # 氨氮读取
│   ├── readTemperature()       # DS18B20 水温
│   ├── readDepth()             # 深度读取（委托给 DepthSensor）
│   ├── enableSensor() / disableSensor()  # 传感器电源控制
│   └── triggerAlarm() / stopAlarm()      # 报警输出
│
├── DepthSensor                 # 深度传感器驱动类
│   ├── MS5837 初始化与校准
│   ├── 压力 / 温度 / 深度计算
│   └── 流体密度可配置（淡水/海水）
│
└── WiFi + PubSubClient          # 网络通信层
    ├── WiFi 自动重连
    ├── MQTT 保活 (5s 间隔)
    └── JSON 数据上报到华为云
```

---

## 📚 使用手册

完整的硬件接线、软件安装、编译烧录、MQTT 配置、传感器校准与故障排查指南请参阅：

[![查看使用手册](https://img.shields.io/badge/📖_查看使用手册-ESP32_水质传感器系统-2ea44f?style=for-the-badge)](doc/ESP32_使用说明手册.md)

---

## 🚀 快速上手

```bash
# 1. 安装 PlatformIO (VSCode 扩展或 CLI)
# 2. 克隆本项目
git clone https://github.com/chinokoyuki/underwaterrobot-water-quality-sensor-mqtt-arduino-esp32.git

# 3. 打开项目
cd underwaterrobot-water-quality-sensor-mqtt-arduino-esp32
code .   # 或用 VSCode 打开

# 4. 编译
pio run

# 5. 烧录（USB-C 连接开发板）
pio run --target upload

# 6. 串口监视
pio device monitor
```

> ⚠️ 烧录前请修改 `src/main.cpp` 中的 WiFi 和华为云 MQTT 配置参数。

---

## 📌 项目信息

| 项目 | 内容 |
|------|------|
| 开发者 | [chinokoyuki](https://github.com/chinokoyuki) |
| 开源协议 | MIT |
| 移植来源 | STM32 版水下机器人水质传感器工程 |
| 主要变更 | MS5837 深度传感器移植、I2C 适配、华为云 MQTT |

---

> 💡 **提示**：详细接线图、引脚分配总表、校准方法及常见排障请务必查看 [使用手册](doc/ESP32_使用说明手册.md) 哦～
