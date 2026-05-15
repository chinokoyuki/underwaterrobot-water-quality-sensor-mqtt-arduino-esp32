# STM32 与 Arduino ESP32 水质传感器工程对比报告

> 生成日期：2026-05-15

---

## 1. 工程概况对比

| 对比项 | STM32 工程（水质传感器二版） | Arduino ESP32 工程 |
|---|---|---|
| **MCU** | STM32F103C8T6 (Cortex-M3, 72MHz) | ESP32-S3 (Xtensa, 240MHz) |
| **开发板** | 自定义最小系统板 | Arduino Nano ESP32 |
| **开发框架** | STM32标准库 (StdPeriph) + Keil MDK | Arduino + PlatformIO |
| **编程语言** | C | C++ (Arduino风格) |
| **编译工具链** | ARMCC (Keil) | Xtensa GCC (ESP-IDF/Arduino) |
| **项目结构** | 扁平目录（Hardware/System/User/Library） | PlatformIO标准结构（src/lib/include） |

---

## 2. 传感器模块对比

### 2.1 共有传感器

| 传感器 | STM32 实现 | ESP32 实现 | 状态 |
|---|---|---|---|
| **pH 传感器** | ADC通道1 (PA1)，软件采样取平均 | A0引脚，`readAverageVoltage()` 采样10次取平均 | ✅ 功能等价 |
| **溶氧 (DO) 传感器** | ADC通道0 (PA0) | 未独立实现（ESP32用TDS替代了DO） | ⚠️ 功能不同 |
| **浊度传感器** | ADC通道2 (PA2) | A2引脚，直接读取ADC原始值 | ✅ 功能等价 |
| **氨氮传感器** | ADC通道3 (PA3) | A3引脚，电压转换+校准系数 | ✅ 功能等价（ESP32有更完善的计算） |
| **DS18B20 水温传感器** | 裸GPIO模拟单总线 (PB0)，手动时序 | OneWire+DallasTemperature库 (A4) | ✅ 功能等价（ESP32用库更简洁） |
| **串口通信** | USART1 (PA9/PA10, 115200bps) | Serial (USB-CDC, 115200bps) | ✅ 功能等价 |

### 2.2 STM32 独有 / 新增传感器

| 传感器 | STM32 实现 | ESP32 移植状态 | 说明 |
|---|---|---|---|
| **深度传感器** | 软件I2C (PB6=SCL, PB7=SDA)，`Depth_Sensor_Read()` 返回模拟数据 | ✅ 已移植 | 改用硬件I2C (GPIO8/GPIO9)，实现了MS5837真实读取逻辑 |

### 2.3 ESP32 独有功能

| 功能 | STM32 | ESP32 | 说明 |
|---|---|---|---|
| **TDS 传感器** | ❌ 无 | ✅ 有 (A1引脚) | ESP32新增，替代了STM32的DO传感器 |
| **WiFi 连接** | ❌ 无 | ✅ 有 | ESP32内置WiFi |
| **MQTT 上报** | ❌ 无 | ✅ 有 (华为云IoT) | 通过PubSubClient实现 |
| **报警蜂鸣器** | ❌ 无 | ✅ 有 (GPIO3) | 水质超标报警 |
| **传感器使能控制** | ❌ 无 | ✅ 有 (GPIO2) | 控制传感器供电 |
| **校准参数掉电存储** | ❌ 无 | ✅ 有 (Preferences/Flash) | 校准参数持久化 |
| **LED 状态控制** | ❌ 无 | ✅ 有 | MQTT远程控制 |

---

## 3. ADC 采集对比

| 对比项 | STM32 | ESP32 |
|---|---|---|
| **ADC 分辨率** | 12 位 (0-4095) | 12 位 (0-4095) |
| **参考电压** | 3.3V | 3.3V |
| **采样方式** | 单次软件触发，多次平均 | `analogRead()`，10次平均 |
| **通道数量** | 4路 (PA0-PA3) | 4路 (A0-A3) |
| **衰减配置** | 无（固定量程） | 11dB衰减 (0-3.9V) |

---

## 4. 温度传感器实现对比

### STM32 DS18B20 实现（裸GPIO）

```c
// 手动时序控制
static uint8_t DS_Reset(void) { ... }       // 复位脉冲
static void DS_Write_Byte(uint8_t dat) { ... } // 写字节
static uint8_t DS_Read_Byte(void) { ... }    // 读字节

float DS18B20_Get_Temp(void) {
    DS_Reset();
    DS_Write_Byte(0xCC);  // 跳过ROM
    DS_Write_Byte(0x44);  // 转换温度
    DS_Reset();
    DS_Write_Byte(0xCC);
    DS_Write_Byte(0xBE);  // 读暂存器
    L = DS_Read_Byte();
    H = DS_Read_Byte();
    temp_raw = (H << 8) | L;
    return (float)temp_raw * 0.0625;
}
```

### ESP32 DS18B20 实现（库封装）

```cpp
// 使用OneWire + DallasTemperature库
OneWire oneWire(TEMP_SENSOR_PIN);
DallasTemperature ds18b20(&oneWire);

float WaterQualitySensor::readTemperature() {
    ds18b20.requestTemperatures();
    float temp = ds18b20.getTempCByIndex(0);
    if (temp < -127.0f) return -1.0f;
    return constrain(temp, 0.0f, 100.0f);
}
```

**对比结论**：两者功能完全等价，ESP32使用Arduino库更简洁可靠。

---

## 5. 深度传感器移植详情

### STM32 原版 (depth_sensor.c)

| 项目 | 内容 |
|---|---|
| 通信方式 | 软件模拟I2C (GPIO位操作) |
| 引脚 | PB6 (SCL), PB7 (SDA) |
| 初始化 | `Depth_Sensor_Init()` — 配置GPIO为推挽输出 |
| 读取 | `Depth_Sensor_Read()` — **返回模拟数据 1.23f** |
| 底层实现 | 完整的I2C Start/Stop/Ack/Send_Byte |
| 问题 | 读取函数未实现真实传感器逻辑 |

### ESP32 移植版 (DepthSensor.h/cpp)

| 项目 | 内容 |
|---|---|
| 通信方式 | 硬件I2C (Wire库) |
| 引脚 | GPIO8 (SCL), GPIO9 (SDA) |
| 初始化 | `DepthSensor.begin()` — 初始化Wire，读取MS5837 PROM校准 |
| 读取 | `DepthSensor.readDepth()` — **实现MS5837真实读取逻辑** |
| 底层实现 | 使用Wire库，包含完整的MS5837压力/温度/深度计算 |
| 改进 | 完成二阶温度补偿，支持淡水/海水密度切换 |

### 移植改动要点

1. **I2C 实现替换**：GPIO模拟I2C → Wire硬件I2C（更稳定、更高效）
2. **引脚映射**：PB6/PB7 → GPIO8/GPIO9
3. **真实读取逻辑**：原版返回模拟数据 → 实现MS5837完整计算（含校准系数读取、ADC转换、二阶温度补偿）
4. **面向对象封装**：C函数 → C++类，支持多实例
5. **错误处理**：增加初始化检测、连接状态检查、错误码返回
6. **扩展功能**：新增压力读取、深度传感器内部温度读取、流体密度设置

---

## 6. 主循环逻辑对比

### STM32 main.c

```c
while (1) {
    // ADC采样20次取平均
    adc_do   = Water_ADC_Read(CH_DO,   20);
    adc_ph   = Water_ADC_Read(CH_PH,   20);
    adc_turb = Water_ADC_Read(CH_TURB, 20);
    adc_nh3  = Water_ADC_Read(CH_NH3,  20);
    // DS18B20温度
    temp_val = DS18B20_Get_Temp();
    // 深度传感器
    depth_val = Depth_Sensor_Read();
    // 串口打印
    printf("...");
    Delay_ms(1000);
}
```

### ESP32 main.cpp

```cpp
void loop() {
    // 传感器使能
    waterSensor.enableSensor();
    // 读取所有传感器
    float phValue = waterSensor.readPH();
    float tdsValue = waterSensor.readTDS();       // 替代DO
    int turbidityValue = waterSensor.readTurbidity();
    float ammoniaValue = waterSensor.readAmmoniaNitrogen();
    float tempValue = waterSensor.readTemperature();
    float depthValue = waterSensor.readDepth();    // 新增
    // 水质判断+报警
    bool isNormal = waterSensor.isWaterQualityNormal(...);
    // MQTT上报
    mqttIntervalPost();
    delay(2000);
}
```

### 主要差异

| 对比项 | STM32 | ESP32 |
|---|---|---|
| 采样次数 | 20次平均 | 10次平均 |
| 循环间隔 | 1000ms | 2000ms |
| 数据输出 | printf (串口) | Serial + MQTT云端上报 |
| 报警机制 | 无 | 水质超标自动报警 |
| 网络功能 | 无 | WiFi + MQTT |

---

## 7. 代码质量对比

| 对比项 | STM32 | ESP32 |
|---|---|---|
| 模块化程度 | 中等（按.c/.h文件拆分） | 高（面向对象，类封装） |
| 错误处理 | 基本无 | 有返回值检查、超时处理 |
| 校准持久化 | 无 | Preferences Flash存储 |
| 可扩展性 | 低（需手动修改底层代码） | 高（类继承/组合） |
| 注释质量 | 一般 | 详细（含移植说明） |

---

## 8. 总结

### 相同点
- 核心水质传感器功能一致（pH、浊度、氨氮、水温）
- ADC采样原理相同（12位分辨率、3.3V参考电压、多次采样取平均）
- 串口输出格式类似

### 不同点
- **STM32 多了**：DO（溶氧）传感器通道（但ADC读取与pH/浊度/氨氮同质）
- **ESP32 多了**：TDS传感器、WiFi/MQTT、报警、校准存储、传感器使能控制
- **深度传感器**：STM32仅有框架（返回模拟数据），ESP32实现了完整的MS5837读取逻辑

### 移植完成状态
| 移植项 | 状态 |
|---|---|
| 深度传感器核心代码 | ✅ 已完成（DepthSensor.h/cpp） |
| 深度传感器集成到WaterQualitySensor | ✅ 已完成 |
| 主循环深度数据读取 | ✅ 已完成 |
| 水质判断增加深度参数 | ✅ 已完成 |
| 串口输出增加深度列 | ✅ 已完成 |
| 编译验证 | ✅ 通过（RAM 10.8%, Flash 13.7%） |
