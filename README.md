# 水下机器人水质传感器系统

> Arduino Nano ESP32 · 多参数水质监测 · 华为云 IoTDA MQTT 上报

基于 **Arduino Nano ESP32** (ESP32-S3) 开发的水质多参数采集与云端上报系统，搭载 **pH / TDS / 浊度 / 溶解氧 / DS18B20 水温 / MS5837 深度** 六合一传感器阵列，支持超标报警、板载 LED 状态指示与华为云 IoT 平台实时数据上报。

---

## 功能特性

- **6 参数水质采集**：pH、TDS、浊度、溶解氧、水温、深度
- **深度传感器扩展**：MS5837 同时输出压力、温度、深度数据
- **超标报警**：pH / TDS / 溶解氧 / 深度超阈值自动触发蜂鸣器
- **云端上报**：MQTT v3.1.1 接入华为云 IoTDA，5 秒周期自动上报 11 个属性
- **板载 LED 指示**：WiFi 连接闪烁、上报中常亮、成功双闪、失败长闪
- **自动重连**：WiFi / MQTT 断线自动恢复，超时自动重启

---

## 技术栈

| 层面 | 技术 | 说明 |
|------|------|------|
| **主控芯片** | ESP32-S3 (Xtenga LX7 双核 240MHz) | Arduino Nano ESP32 开发板 |
| **开发框架** | PlatformIO + Arduino | 跨平台嵌入式构建系统 |
| **编程语言** | C++11 (Arduino 风格) | 面向对象封装传感器类 |
| **通信协议** | WiFi (802.11 b/g/n) + MQTT (PubSubClient) | 无线连接华为云 IoTDA |
| **云平台** | 华为云 IoT 设备接入 (IoTDA) | 属性上报 + 命令下发 |

### 依赖库

| 库名 | 用途 | 版本 |
|------|------|------|
| OneWire | DS18B20 单总线通信 | ^2.3.8 |
| DallasTemperature | DS18B20 温度读取 | ^4.0.5 |
| PubSubClient | MQTT 客户端协议实现 | ^2.8 |
| WiFi | ESP32 WiFi 驱动 | ^2.0.0 |
| Wire | ESP32 I2C 驱动 | ^2.0.0 |

---

## 硬件接线

### 传感器引脚分配

| 传感器 | 测量参数 | 接口类型 | 引脚 |
|--------|---------|---------|------|
| pH 电极模块 | 酸碱度 (0-14) | 模拟输入 | A0 |
| TDS 笔模块 | 溶解性固体 (ppm) | 模拟输入 | A1 |
| 浊度传感器 | 水体浊度 (NTU) | 模拟输入 | A2 |
| 溶解氧传感器 | 溶解氧浓度 (mg/L) | 模拟输入 | A3 |
| DS18B20 | 水温 (℃) | 单总线 (OneWire) | A4 |
| MS5837 | 深度 (m) / 压力 (mbar) | I2C | D8(SCL) / D9(SDA) |

### 控制引脚

| 功能 | 引脚 | 类型 |
|------|------|------|
| 传感器使能 | GPIO2 | 数字输出 (高电平使能) |
| 报警蜂鸣器 | GPIO3 | 数字输出 (高电平触发) |
| 板载 LED | GPIO13 (LED_BUILTIN) | 数字输出 |

---

## 云端配置

### 华为云 IoTDA 物模型

项目上报的 **service_id** 为 `water_quality`，包含以下 10 个属性：

| 属性名 (物模型) | 中文名 | 数据类型 | 对应传感器 |
|---|---|---|---|
| `pH` | pH 值 | float | pH 电极 |
| `TDS` | TDS | float | TDS 模块 |
| `turbidity` | 浊度 | float | 浊度传感器 |
| `dissolvedOxygen` | 溶解氧 | float | 溶解氧传感器 |
| `waterTemp` | 水温 | float | DS18B20 |
| `depth` | 深度 | float | MS5837 |
| `depthPressure` | 深度压力 | float | MS5837 |
| `depthSensorTemp` | 深度传感器温度 | float | MS5837 |
| `waterQualityNormal` | 水质是否正常 | boolean | 综合判断 |
| `depthSensorOnline` | 深度传感器在线 | boolean | MS5837 |

> 属性名必须与华为云 IoTDA 控制台物模型定义**完全一致**（含大小写），否则上报数据无法被平台识别。

### MQTT 上报格式

```
Topic: $oc/devices/{DeviceId}/sys/properties/report

Payload:
{
  "services": [{
    "service_id": "water_quality",
    "properties": {
      "pH": 7.00,
      "TDS": 96.2,
      "turbidity": 15.32,
      "dissolvedOxygen": 7.85,
      "waterTemp": 25.0,
      "depth": 1.50,
      "depthPressure": 1013.0,
      "depthSensorTemp": 24.5,
      "waterQualityNormal": true,
      "depthSensorOnline": true
    }
  }]
}
```

---

## 板载 LED 状态指示

| 状态 | LED 表现 |
|------|---------|
| WiFi 连接中 | 快闪（500ms 间隔） |
| MQTT 上报中 | 常亮 |
| 上报成功 | 短闪 2 次（80ms） |
| 上报失败 | 长闪 1 次（300ms） |
| 空闲 | 灭 |

---

## 软件架构

```
src/
├── main.cpp                   # 主程序（WiFi / MQTT / 主循环 / LED 指示）
├── WaterQualitySensor.h/cpp   # 水质传感器类（pH / TDS / 浊度 / 溶解氧 / 水温 / 深度）
└── DepthSensor.h/cpp          # 深度传感器类（MS5837 I2C 驱动）

main.cpp
├── wifiInit()                 # WiFi 连接 + MQTT 服务器配置
├── mqttCheckConnect()         # MQTT 自动连接 + 状态诊断
├── mqttCallback()             # 接收平台下发命令
├── readSensorData()           # 传感器数据采集
├── mqttIntervalPost()         # 数据上报（beginPublish 流式发送）
├── serialPrintData()          # 串口表格输出
├── setup()                    # 初始化
└── loop()                     # 主循环：采集 → 上报 → MQTT 消息处理
```

---

## 快速上手

```bash
# 1. 安装 PlatformIO (VSCode 扩展或 CLI)

# 2. 克隆本项目
git clone https://github.com/chinokoyuki/underwaterrobot-water-quality-sensor-mqtt-arduino-esp32.git
cd underwaterrobot-water-quality-sensor-mqtt-arduino-esp32

# 3. 修改配置（src/main.cpp）
#    - WIFI_SSID / WIFI_PASSWD：WiFi 名称和密码
#    - 华为云 MQTT 凭据：DEVICE_ID / MQTT_SERVER / CLIENT_ID / MQTT_PASSWD

# 4. 编译
pio run

# 5. 烧录（USB-C 连接开发板）
pio run --target upload

# 6. 串口监视（波特率 115200）
pio device monitor
```

> 烧录前必须修改 `src/main.cpp` 中的 WiFi 和华为云 MQTT 配置参数。MQTT 密码生成方式详见 [MQTT 对接手册](doc/MQTT华为云对接手册.md)。

---

## 文档

| 文档 | 说明 |
|------|------|
| [使用说明手册](doc/ESP32_使用说明手册.md) | 硬件接线、安装编译、传感器校准、故障排查 |
| [MQTT 华为云对接手册](doc/MQTT华为云对接手册.md) | MQTT 密码生成、物模型配置、数据格式说明 |

---

## 已知问题与注意事项

1. **PubSubClient 大包发送**：ESP32 的 `WiFiClient.write()` 可能分片发送，导致 `publish()` 返回 `false`。本项目已改用 `beginPublish()` + `print()` + `endPublish()` 流式发送方式解决。
2. **MQTT 心跳间隔**：华为云 IoTDA 要求 KeepAlive >= 30 秒，PubSubClient 默认 15 秒会导致连接被拒（state code 2），已在代码中设置 `setKeepAlive(60)`。
3. **MQTT 密码算法**：华为云 IoTDA 的 MQTT 密码使用 `hmac_sha256(key=TimeStamp, message=DeviceSecret)`，**key 是时间戳，message 是设备密钥**，切勿写反。
4. **物模型一致性**：上报 JSON 中的属性名必须与华为云控制台物模型定义完全一致（含大小写），否则平台无法识别数据。

---
