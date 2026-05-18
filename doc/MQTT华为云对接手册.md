# MQTT 对接华为云 IoTDA 手册

> 版本：V1.0
> 更新日期：2026-05-15
> 适用平台：Arduino Nano ESP32 + 华为云 IoTDA

---

## 目录

1. [概述](#1-概述)
2. [华为云 IoTDA 前置准备](#2-华为云-iotda-前置准备)
3. [创建产品与物模型](#3-创建产品与物模型)
4. [注册设备与获取连接参数](#4-注册设备与获取连接参数)
5. [MQTT 连接参数详解](#5-mqtt-连接参数详解)
6. [Topic 与 Payload 格式](#6-topic-与-payload-格式)
7. [设备端代码配置](#7-设备端代码配置)
8. [数据上报流程](#8-数据上报流程)
9. [平台端验证与调试](#9-平台端验证与调试)
10. [常见问题与排障](#10-常见问题与排障)
11. [附录](#11-附录)

---

## 1. 概述

本文档详细说明如何将 ESP32 水质传感器系统通过 MQTT 协议对接华为云物联网平台（IoT Data Access, IoTDA）。内容包括：

- 华为云 IoTDA 上产品、物模型的创建方法
- 设备注册与 MQTT 连接参数的获取
- MQTT 通信的 Topic 和 Payload 格式规范
- 设备端代码配置说明
- 数据上报的完整流程与验证方法

### 1.1 系统架构

```
┌──────────────┐      WiFi       ┌──────────────┐     MQTT      ┌──────────────────┐
│ ESP32 传感器  │ ──────────────→ │  路由器/AP    │ ────────────→ │ 华为云 IoTDA      │
│ (MQTT Client)│  2.4GHz WiFi    │              │  1883端口     │ (MQTT Broker)     │
└──────────────┘                 └──────────────┘              └──────────────────┘
                                                                  │
                                                                  ▼
                                                          ┌──────────────────┐
                                                          │ 应用服务器/       │
                                                          │ 数据可视化        │
                                                          └──────────────────┘
```

### 1.2 通信协议

| 项目 | 值 |
|---|---|
| 传输协议 | MQTT v3.1.1 |
| 传输层 | TCP（明文，端口 1883）|
| MQTT 库 | PubSubClient (knolleary/PubSubClient@^2.8) |
| 数据格式 | JSON |

> ⚠️ 生产环境建议使用 TLS 加密（端口 8883），需额外配置证书。本文档使用明文传输便于开发调试。

---

## 2. 华为云 IoTDA 前置准备

### 2.1 注册华为云账号

1. 访问 [华为云官网](https://www.huaweicloud.com/)
2. 点击"注册"创建账号
3. 完成实名认证

### 2.2 开通 IoTDA 服务

1. 登录华为云控制台
2. 搜索"设备接入 IoTDA"或直接访问 [IoTDA 控制台](https://console.huaweicloud.com/iotda/)
3. 点击"开通"或"免费试用"（基础版免费，每月100万条消息）
4. 选择区域（建议选择与设备最近的区域，本项目使用 **华南-广州** `cn-south-1`）

### 2.3 获取接入信息

1. 进入 IoTDA 控制台 → "总览"页面
2. 记录以下信息：
   - **接入域名**：如 `cbd1b71b46.st1.iotda-device.cn-south-1.myhuaweicloud.com`
   - **MQTT 端口**：1883（TCP）/ 8883（TLS）

---

## 3. 创建产品与物模型

### 3.1 创建产品

1. 进入 IoTDA 控制台 → "产品" → "创建产品"
2. 填写产品信息：

| 字段 | 值 | 说明 |
|---|---|---|
| 所属空间 | 默认空间 | 或选择自定义空间 |
| 产品名称 | WaterQualitySensor | 自定义，便于识别 |
| 协议类型 | MQTT | |
| 数据格式 | JSON | |
| 厂商名称 | 自定义 | 如 "UnderwaterRobot" |
| 设备类型 | Sensor | 自定义 |

3. 点击"确定"创建产品

### 3.2 定义物模型

物模型定义了设备与云端交互的数据格式，是 MQTT 通信的核心配置。

1. 在产品列表中点击刚创建的产品名称
2. 选择"功能定义"选项卡
3. 点击"新增服务"，创建服务：

| 字段 | 值 | 说明 |
|---|---|---|
| 服务ID | `water_quality` | **必须与代码中的 service_id 一致** |
| 服务类型 | Sensor | |
| 服务描述 | 水质监测数据 | |

4. 在服务下添加以下属性（点击"新增属性"）：

#### 属性列表

| 属性名 | 标识符 | 数据类型 | 取值范围 | 步长 | 单位 | 读写类型 | 说明 |
|---|---|---|---|---|---|---|---|
| pH值 | `pH` | decimal(浮点型) | 0 ~ 14 | 0.01 | — | 只读 | pH 酸碱度 |
| TDS值 | `TDS` | decimal(浮点型) | 0 ~ 2000 | 0.1 | ppm | 只读 | 溶解性固体总量 |
| 浊度 | `turbidity` | int(整数型) | 0 ~ 4095 | 1 | — | 只读 | ADC 原始值 |
| 氨氮浓度 | `ammoniaNitrogen` | decimal(浮点型) | 0 ~ 50 | 0.01 | mg/L | 只读 | 氨氮含量 |
| 水温 | `waterTemp` | decimal(浮点型) | 0 ~ 100 | 0.1 | ℃ | 只读 | DS18B20 水温 |
| 水深 | `depth` | decimal(浮点型) | 0 ~ 30 | 0.01 | m | 只读 | MS5837 深度 |
| 深度压力 | `depthPressure` | decimal(浮点型) | 0 ~ 30000 | 0.1 | mbar | 只读 | 深度传感器压力 |
| 深度温度 | `depthSensorTemp` | decimal(浮点型) | -40 ~ 85 | 0.1 | ℃ | 只读 | 深度传感器内部温度 |
| 水质状态 | `waterQualityNormal` | boolean(布尔型) | — | — | — | 只读 | true=正常, false=超标 |
| 深度传感器状态 | `depthSensorOnline` | boolean(布尔型) | — | — | — | 只读 | true=在线, false=离线 |

5. 点击"确定"保存物模型

> ⚠️ **关键提醒**：属性标识符（如 `pH`、`TDS` 等）**必须与代码中的 JSON 字段名完全一致**，否则平台无法解析数据。

### 3.3 物模型 JSON 导入（快捷方式）

也可通过 JSON 批量导入物模型，点击"动态注册"旁的"导入"按钮，粘贴以下 JSON：

```json
{
  "services": [{
    "service_id": "water_quality",
    "service_type": "Sensor",
    "description": "水质监测数据",
    "properties": [
      {"property_name": "pH值", "identifier": "pH", "data_type": "decimal", "range": [0, 14], "step": 0.01, "method": "R"},
      {"property_name": "TDS值", "identifier": "TDS", "data_type": "decimal", "range": [0, 2000], "step": 0.1, "unit": "ppm", "method": "R"},
      {"property_name": "浊度", "identifier": "turbidity", "data_type": "int", "range": [0, 4095], "step": 1, "method": "R"},
      {"property_name": "氨氮浓度", "identifier": "ammoniaNitrogen", "data_type": "decimal", "range": [0, 50], "step": 0.01, "unit": "mg/L", "method": "R"},
      {"property_name": "水温", "identifier": "waterTemp", "data_type": "decimal", "range": [0, 100], "step": 0.1, "unit": "℃", "method": "R"},
      {"property_name": "水深", "identifier": "depth", "data_type": "decimal", "range": [0, 30], "step": 0.01, "unit": "m", "method": "R"},
      {"property_name": "深度压力", "identifier": "depthPressure", "data_type": "decimal", "range": [0, 30000], "step": 0.1, "unit": "mbar", "method": "R"},
      {"property_name": "深度温度", "identifier": "depthSensorTemp", "data_type": "decimal", "range": [-40, 85], "step": 0.1, "unit": "℃", "method": "R"},
      {"property_name": "水质状态", "identifier": "waterQualityNormal", "data_type": "boolean", "method": "R"},
      {"property_name": "深度传感器状态", "identifier": "depthSensorOnline", "data_type": "boolean", "method": "R"}
    ]
  }]
}
```

---

## 4. 注册设备与获取连接参数

### 4.1 注册设备

1. 进入 IoTDA 控制台 → "设备" → "所有设备" → "注册设备"
2. 填写设备信息：

| 字段 | 值 | 说明 |
|---|---|---|
| 所属产品 | WaterQualitySensor | 选择 3.1 创建的产品 |
| 设备标识码 | 自定义（如设备序列号） | 全局唯一 |
| 设备名称 | ESP32_WaterSensor_01 | 自定义 |
| 密钥 | 自动生成或自定义 | 用于 MQTT 认证 |

3. 点击"确定"注册设备

### 4.2 获取 MQTT 连接参数

设备注册成功后，记录以下参数：

| 参数 | 在代码中的宏定义 | 获取位置 |
|---|---|---|
| 设备ID | `DEVICE_ID` | 设备详情页 → "设备ID" |
| 密钥 | `DEVICE_SECRET` | 注册设备时生成（仅显示一次） |
| MQTT 域名 | `MQTT_SERVER` | IoTDA 总览页 → 接入信息 |
| MQTT 端口 | `MQTT_PORT` | 1883 |
| Client ID | `CLIENT_ID` | 见下方生成规则 |
| 用户名 | `MQTT_USERNAME` | 见下方生成规则 |
| 密码 | `MQTT_PASSWD` | 见下方生成规则 |

### 4.3 MQTT 认证参数生成规则

华为云 IoTDA 使用 MQTT 的 Client ID、Username、Password 进行设备认证，生成规则如下：

#### ClientID

```
格式: {DeviceId}_0_0_{TimeStamp}
示例: 69632991c00ccb6d4b32b204_ABX00083_CN_1_0_0_2026011206
```

- `DeviceId`：设备ID
- `_0_0_`：固定格式（密钥类型_时间戳格式_签名算法，0=设备密钥,0=不校验时间戳,0=HMAC-SHA256）
- `TimeStamp`：自定义时间戳（格式：YYYYMMDDHH），不影响连接

#### Username

```
格式: {DeviceId}
示例: 69632991c00ccb6d4b32b204_ABX00083_CN_1
```

与设备 ID 相同。

#### Password

密码通过 HMAC-SHA256 算法计算：

```
Password = hmac_sha256(key=TimeStamp, message=DeviceSecret)
```

> ⚠️ **注意**：HMAC 的 key 是时间戳，message 是设备密钥，**不要搞反**！
> 错误写法：`hmac_sha256(key=DeviceSecret, message=TimeStamp)` ← 这样算出的密码无法连接！

其中：
- `TimeStamp`：ClientID 中的时间戳部分（格式 `YYYYMMDDHH`，如 `2026051802`）
- `DeviceSecret`：设备密钥（注册时生成）
- 输出格式：64 位十六进制字符串

**生成方法（推荐使用华为云在线工具）：**

1. 访问 [华为云 IoTDA MQTT 密码生成工具](https://iot-tool.obs-website.cn-north-4.myhuaweicloud.com/)
2. 输入 Device ID 和 Device Secret
3. 点击"Generate"，自动生成 ClientID、Username、Password

**手动生成（Python 脚本）：**

```python
import hmac
import hashlib

def generate_password(device_secret, timestamp):
    """生成华为云 IoTDA MQTT 密码
    注意: key=timestamp, message=device_secret (不要搞反！)
    """
    return hmac.new(
        timestamp.encode('utf-8'),       # key = 时间戳
        device_secret.encode('utf-8'),   # message = 设备密钥
        hashlib.sha256
    ).hexdigest()

# 示例
device_secret = "你的设备密钥"
timestamp = "2026051802"
password = generate_password(device_secret, timestamp)
print(f"Password: {password}")
```

---

## 5. MQTT 连接参数详解

### 5.1 参数总览

| 参数 | 宏定义 | 示例值 | 说明 |
|---|---|---|---|
| MQTT Broker 地址 | `MQTT_SERVER` | `cbd1b71b46.st1.iotda-device.cn-south-1.myhuaweicloud.com` | IoTDA 实例接入域名 |
| MQTT Broker 端口 | `MQTT_PORT` | `1883` | TCP 明文端口 |
| Client ID | `CLIENT_ID` | `69632991c00ccb6d4b32b204_ABX00083_CN_1_0_0_2026011206` | 包含设备ID和时间戳 |
| Username | `MQTT_USERNAME` | `69632991c00ccb6d4b32b204_ABX00083_CN_1` | 通常等于设备ID |
| Password | `MQTT_PASSWD` | `8c834a00f2633ce36a07455f812ef84a...` | HMAC-SHA256 计算值 |
| Keep Alive | — | 60 秒 | PubSubClient 默认值 |

### 5.2 连接流程

```
ESP32 上电
    ↓
WiFi 连接（STA 模式，超时30秒重启）
    ↓
设置 MQTT 服务器和端口
    ↓
MQTT Connect（携带 ClientID/Username/Password）
    ↓
连接成功 → 进入主循环
连接失败 → 打印状态码，下次循环重试
```

### 5.3 MQTT 连接状态码

| 状态码 | 含义 | 解决方法 |
|---|---|---|
| -4 | 连接超时 | 检查网络连通性 |
| -3 | 连接断开 | 检查网络稳定性 |
| -2 | 连接失败 | 检查 MQTT_SERVER 地址 |
| -1 | 客户端异常 | 检查内存是否充足 |
| 0 | 连接成功 | — |
| 2 | ClientID/凭证错误 | 检查 ClientID 格式和 Password |
| 4 | 用户名/密码错误 | 检查 Username 和 Password |
| 5 | 未授权 | 检查设备是否已在华为云注册 |

---

## 6. Topic 与 Payload 格式

### 6.1 属性上报（设备 → 平台）

这是本项目使用的主要通信方式，设备主动向平台上报传感器数据。

**Topic:**

```
$oc/devices/{device_id}/sys/properties/report
```

示例：
```
$oc/devices/69632991c00ccb6d4b32b204_ABX00083_CN_1/sys/properties/report
```

**Payload:**

```json
{
  "services": [{
    "service_id": "water_quality",
    "properties": {
      "pH": 7.15,
      "TDS": 189.3,
      "turbidity": 856,
      "ammoniaNitrogen": 2.45,
      "waterTemp": 24.8,
      "depth": 0.32,
      "depthPressure": 1013.5,
      "depthSensorTemp": 24.5,
      "waterQualityNormal": true,
      "depthSensorOnline": true
    }
  }]
}
```

**格式要点：**
- `services` 为数组，可同时上报多个服务的数据
- `service_id` 必须与物模型中定义的服务 ID 一致
- `properties` 中的字段名必须与物模型中的属性标识符一致
- float 类型值不可用引号包裹，boolean 值使用 `true`/`false`（小写）

### 6.2 属性设置（平台 → 设备）

平台可通过该 Topic 向设备下发属性设置命令。

**Topic:**

```
$oc/devices/{device_id}/sys/properties/set
```

**Payload 示例：**

```json
{
  "services": [{
    "service_id": "water_quality",
    "properties": {
      "depthSensorOnline": false
    }
  }]
}
```

> 💡 当前版本设备端仅打印接收到的消息，未实现命令执行逻辑，可按需扩展。

### 6.3 命令下发（平台 → 设备）

平台可下发自定义命令。

**Topic:**

```
$oc/devices/{device_id}/sys/commands/#/#
```

**Payload 示例：**

```json
{
  "command_name": "setAlarm",
  "paras": {
    "enabled": true
  }
}
```

### 6.4 其他常用 Topic

| Topic | 方向 | 用途 |
|---|---|---|
| `$oc/devices/{device_id}/sys/properties/report` | 设备→平台 | 属性上报 |
| `$oc/devices/{device_id}/sys/properties/set` | 平台→设备 | 属性设置 |
| `$oc/devices/{device_id}/sys/properties/get/response/#` | 平台→设备 | 属性查询响应 |
| `$oc/devices/{device_id}/sys/commands/#` | 平台→设备 | 命令下发 |
| `$oc/devices/{device_id}/sys/commands/response/#` | 设备→平台 | 命令响应 |
| `$oc/devices/{device_id}/sys/events/up` | 设备→平台 | 事件上报 |
| `$oc/devices/{device_id}/sys/desc/report` | 设备→平台 | 设备影子上报 |

---

## 7. 设备端代码配置

### 7.1 修改连接参数

打开 `src/main.cpp`，修改以下宏定义：

```cpp
/* 华为云 IoTDA 设备配置 */
#define DEVICE_ID         "你的设备ID"
#define DEVICE_SECRET     "你的设备密钥"

/* WiFi 配置 */
#define WIFI_SSID         "你的WiFi名称"
#define WIFI_PASSWD       "你的WiFi密码"

/* 华为云 MQTT 连接参数 */
#define MQTT_SERVER       "你的华为云MQTT接入域名"
#define MQTT_PORT         1883
#define MQTT_USERNAME     "你的MQTT用户名"
#define CLIENT_ID         "你的ClientID"
#define MQTT_PASSWD       "你的MQTT密码"
```

### 7.2 调整上报周期

```cpp
#define UPLOAD_INTERVAL_MS  5000   // 数据上报间隔（毫秒），默认5秒
#define SENSOR_INTERVAL_MS  2000   // 传感器采集间隔（毫秒），默认2秒
```

- 上报间隔不宜过短，华为云 IoTDA 基础版有消息条数限制
- 采集间隔和上报间隔独立控制，采集更频繁但上报可适当减少

### 7.3 修改物模型服务 ID

如果物模型中使用不同的服务 ID，需同步修改：

```cpp
#define ALINK_BODY_FORMAT  "{\"services\":[{\"service_id\":\"你的服务ID\",\"properties\":%s}]}"
```

### 7.4 代码结构说明

```
main.cpp
├── 宏定义区域
│   ├── 设备配置（DEVICE_ID, DEVICE_SECRET）
│   ├── WiFi 配置（WIFI_SSID, WIFI_PASSWD）
│   ├── MQTT 连接参数（MQTT_SERVER, CLIENT_ID, MQTT_PASSWD）
│   ├── Topic 和 Payload 格式
│   └── 周期配置（UPLOAD_INTERVAL_MS, SENSOR_INTERVAL_MS）
│
├── SensorData 结构体
│   └── 缓存所有传感器数据，确保采集和上报使用同一份数据
│
├── wifiInit()          → WiFi 连接（含超时重启）
├── mqttCheckConnect()  → MQTT 连接维持与自动重连
├── mqttCallback()      → MQTT 消息回调（接收平台下发命令）
├── readSensorData()    → 采集所有传感器数据到 SensorData
├── mqttIntervalPost()  → 构建JSON并上报到华为云
├── serialPrintData()   → 串口表格输出传感器数据
│
├── setup()             → 初始化：串口→传感器→WiFi→MQTT
└── loop()              → 主循环：采集→上报→处理消息
```

---

## 8. 数据上报流程

### 8.1 完整流程

```
1. 设备上电，初始化串口和传感器
      ↓
2. 连接 WiFi（STA模式，2.4GHz）
   ├─ 成功 → 打印 IP 地址
   └─ 超时(30s) → 自动重启
      ↓
3. 设置 MQTT 服务器地址和端口
      ↓
4. MQTT 连接（携带 ClientID/Username/Password）
   ├─ 成功 → 进入主循环
   └─ 失败 → 打印状态码，下次循环重试
      ↓
5. 主循环（持续运行）
   ├─ 每 2s: 采集传感器数据 → 串口输出 → 水质判断 → 报警控制
   ├─ 每 5s: 构建 JSON → MQTT 发布 → 打印上报结果
   └─ 持续: 检查 MQTT 连接 → 处理下行消息
```

### 8.2 数据采集到上报的映射

```
传感器原始数据                SensorData 结构体            JSON 属性
─────────────              ─────────────────           ────────────
readPH()            →      sensorData.ph          →    "pH"
readTDS()           →      sensorData.tds         →    "TDS"
readTurbidity()     →      sensorData.turbidity   →    "turbidity"
readAmmoniaNitrogen() →    sensorData.ammonia     →    "ammoniaNitrogen"
readTemperature()   →      sensorData.temperature →    "waterTemp"
readDepth()         →      sensorData.depth       →    "depth"
readDepthPressure() →      sensorData.depthPressure →  "depthPressure"
readDepthTemperature() →   sensorData.depthTemp   →    "depthSensorTemp"
isWaterQualityNormal() →   sensorData.isNormal    →    "waterQualityNormal"
isDepthSensorConnected() → sensorData.depthSensorOk →  "depthSensorOnline"
```

---

## 9. 平台端验证与调试

### 9.1 查看设备在线状态

1. 登录华为云 IoTDA 控制台
2. "设备" → "所有设备"
3. 查看设备状态：
   - 🟢 **在线**：设备已成功连接
   - 🔴 **离线**：设备未连接或连接断开
   - 🟡 **未激活**：设备从未连接过

### 9.2 查看上报数据

1. 点击设备名称进入设备详情
2. 选择"设备影子"选项卡
3. 查看最新的属性值和更新时间

### 9.3 使用 MQTT.fx 调试

推荐使用 [MQTT.fx](http://mqttfx.jensd.de/) 或 [MQTTX](https://mqttx.app/) 进行 MQTT 调试。

**连接配置：**

| 配置项 | 值 |
|---|---|
| Broker Address | IoTDA 接入域名 |
| Broker Port | 1883 |
| Client ID | 与设备端相同 |
| User Credentials - User Name | 设备 ID |
| User Credentials - Password | HMAC-SHA256 计算的密码 |

**订阅 Topic：**

```
$oc/devices/69632991c00ccb6d4b32b204_ABX00083_CN_1/sys/properties/set
```

**发布测试消息：**

Topic: `$oc/devices/69632991c00ccb6d4b32b204_ABX00083_CN_1/sys/properties/report`

Payload:
```json
{
  "services": [{
    "service_id": "water_quality",
    "properties": {
      "pH": 7.00,
      "TDS": 150.0,
      "turbidity": 500,
      "ammoniaNitrogen": 1.50,
      "waterTemp": 25.0,
      "depth": 0.50,
      "depthPressure": 1013.25,
      "depthSensorTemp": 25.0,
      "waterQualityNormal": true,
      "depthSensorOnline": true
    }
  }]
}
```

### 9.4 串口调试输出

设备端 MQTT 上报时会输出详细调试信息：

```
========== MQTT 数据上报 ==========
  pH值:       7.15
  TDS:        189.3
  浊度(ADC):  856
  氨氮:       2.45
  水温:       24.8
  深度:       0.32
  深度压力:   1013.5
  深度温度:   24.5
  水质状态:   正常
  深度传感器: 在线
  上报结果:   成功
===================================
```

### 9.5 华为云日志查看

1. IoTDA 控制台 → "运维监控" → "日志"
2. 可查看设备上下线、消息收发、异常等日志
3. 支持按设备ID、时间范围筛选

---

## 10. 常见问题与排障

### Q1: MQTT 连接失败，状态码 2

**原因**：ClientID 格式错误。
**解决**：ClientID 格式必须为 `{DeviceId}_0_0_{TimeStamp}`，确保下划线和时间戳正确。

### Q2: MQTT 连接失败，状态码 4

**原因**：用户名或密码错误。
**解决**：
1. Username 通常等于设备 ID
2. Password 需要通过 HMAC-SHA256(DeviceSecret, TimeStamp) 计算
3. 使用华为云在线工具重新生成：https://iot-tool.obs-website.cn-north-4.myhuaweicloud.com/

### Q3: 连接成功但上报后平台无数据

**原因**：Topic 或 Payload 格式不正确。
**解决**：
1. 检查 Topic 格式：`$oc/devices/{device_id}/sys/properties/report`
2. 检查 `service_id` 是否与物模型中定义的一致
3. 检查属性标识符是否与物模型完全匹配（区分大小写）
4. 检查 JSON 格式是否合法（float 值不加引号，boolean 使用小写 true/false）

### Q4: 设备频繁掉线

**原因**：网络不稳定或 Keep Alive 超时。
**解决**：
1. 检查 WiFi 信号强度
2. 确保主循环中调用 `client.loop()` 处理心跳
3. PubSubClient 默认 Keep Alive 为 15 秒，可在 `client.setServer()` 后调整

### Q5: 上报数据与物模型不匹配

**原因**：属性标识符拼写不一致。
**解决**：对照下表检查代码中的 JSON 字段名和物模型标识符：

| 代码中字段 | 物模型标识符 | 必须一致 |
|---|---|---|
| `"pH"` | `pH` | ✅ |
| `"TDS"` | `TDS` | ✅ |
| `"turbidity"` | `turbidity` | ✅ |
| `"ammoniaNitrogen"` | `ammoniaNitrogen` | ✅ |
| `"waterTemp"` | `waterTemp` | ✅ |
| `"depth"` | `depth` | ✅ |
| `"depthPressure"` | `depthPressure` | ✅ |
| `"depthSensorTemp"` | `depthSensorTemp` | ✅ |
| `"waterQualityNormal"` | `waterQualityNormal` | ✅ |
| `"depthSensorOnline"` | `depthSensorOnline` | ✅ |

### Q6: 华为云控制台看不到设备数据更新

**原因**：设备影子未开启或数据格式错误。
**解决**：
1. 确认设备状态为"在线"
2. 查看"设备影子"中 `reported` 字段是否有值
3. 查看 IoTDA 运维日志确认消息是否被接收

### Q7: WiFi 连接超时不断重启

**原因**：WiFi 名称或密码错误，或信号太弱。
**解决**：
1. 确认 `WIFI_SSID` 和 `WIFI_PASSWD` 正确
2. ESP32 仅支持 2.4GHz WiFi，不支持 5GHz
3. 确保路由器距离合适

### Q8: 上报的 float 值精度异常（如 7.150001）

**原因**：浮点数精度问题。
**解决**：代码中使用 `%.2f` 和 `%.1f` 格式化控制精度，正常情况下不会出现此问题。如出现，检查 `snprintf` 格式字符串是否正确。

---

## 11. 附录

### 附录 A: 华为云 IoTDA 相关链接

| 资源 | 链接 |
|---|---|
| IoTDA 控制台 | https://console.huaweicloud.com/iotda/ |
| MQTT 密码生成工具 | https://iot-tool.obs-website.cn-north-4.myhuaweicloud.com/ |
| IoTDA 开发者文档 | https://support.huaweicloud.com/devg-iothub/iot_02_1002.html |
| MQTT 协议规范 | https://support.huaweicloud.com/api-iothub/iot_06_v5_3002.html |
| 设备接入 SDK | https://support.huaweicloud.com/sdk-iothub/iot_06_v3003.html |

### 附录 B: 完整的 MQTT 报文示例

**CONNECT 报文参数：**

```
Broker:   cbd1b71b46.st1.iotda-device.cn-south-1.myhuaweicloud.com:1883
ClientID: 69632991c00ccb6d4b32b204_ABX00083_CN_1_0_0_2026011206
Username: 69632991c00ccb6d4b32b204_ABX00083_CN_1
Password: 8c834a00f2633ce36a07455f812ef84a3171955848ceda2534b899f8b9736e1e
KeepAlive: 60s
CleanSession: 1
```

**PUBLISH 报文示例：**

```
Topic:   $oc/devices/69632991c00ccb6d4b32b204_ABX00083_CN_1/sys/properties/report
QoS:     0
Retain:  false
Payload:
{
  "services": [{
    "service_id": "water_quality",
    "properties": {
      "pH": 7.15,
      "TDS": 189.3,
      "turbidity": 856,
      "ammoniaNitrogen": 2.45,
      "waterTemp": 24.8,
      "depth": 0.32,
      "depthPressure": 1013.5,
      "depthSensorTemp": 24.5,
      "waterQualityNormal": true,
      "depthSensorOnline": true
    }
  }]
}
```

### 附录 C: 快速对接 Checklist

- [ ] 华为云账号注册并实名认证
- [ ] 开通 IoTDA 服务（选择区域）
- [ ] 创建产品（协议=MQTT，数据格式=JSON）
- [ ] 定义物模型（service_id=`water_quality`，10个属性）
- [ ] 注册设备，获取 Device ID 和 Device Secret
- [ ] 生成 MQTT 连接参数（ClientID/Username/Password）
- [ ] 修改 main.cpp 中的连接参数
- [ ] 编译烧录固件
- [ ] 查看串口输出确认 WiFi/MQTT 连接成功
- [ ] 在华为云控制台验证设备在线和数据上报

### 附录 D: 安全建议

1. **生产环境使用 TLS**：将端口改为 8883，配置 CA 证书
2. **定期更新密钥**：定期重新生成 MQTT Password
3. **不硬编码凭据**：生产环境建议使用 NVS/Preferences 存储敏感信息
4. **限制上报频率**：避免短时间内大量消息上报
5. **网络异常处理**：当前代码已实现 WiFi 超时重启和 MQTT 自动重连
