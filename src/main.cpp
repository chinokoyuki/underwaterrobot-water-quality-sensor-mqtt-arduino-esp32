#include "WaterQualitySensor.h"
#include <PubSubClient.h>
#include <WiFi.h>

/* ========== 华为云 IoTDA 设备配置 ========== */
#define DEVICE_ID         "你的设备ID"
#define DEVICE_SECRET     "你的设备密钥"

/* ========== WiFi 配置 ========== */
#define WIFI_SSID         "你的WiFi名称"
#define WIFI_PASSWD       "你的WiFi密码"

/* ========== 华为云 MQTT 连接参数 ========== */
#define MQTT_SERVER       "你的华为云MQTT地址"
#define MQTT_PORT         1883
#define MQTT_USERNAME     "你的MQTT用户名（通常与DEVICE_ID相同）"
#define CLIENT_ID         "你的客户端ID"
#define MQTT_PASSWD       "你的MQTT密码"

/* ========== MQTT Topic 和 Payload 格式 ========== */
// 属性上报 Topic（设备 -> 平台）
#define ALINK_TOPIC_PROP_POST     "$oc/devices/" DEVICE_ID "/sys/properties/report"
// 属性设置响应 Topic（设备 -> 平台，响应平台下发命令）
#define ALINK_TOPIC_PROP_SET      "$oc/devices/" DEVICE_ID "/sys/properties/set"
// 命令下发 Topic（平台 -> 设备）
#define ALINK_TOPIC_COMMAND       "$oc/devices/" DEVICE_ID /sys/commands/#
// 属性上报 Payload 模板（华为云 IoTDA 标准格式）
// service_id 对应华为云控制台创建的"服务"标识
#define ALINK_BODY_FORMAT         "{\"services\":[{\"service_id\":\"water_quality\",\"properties\":%s}]}"

/* ========== 上报周期配置 ========== */
#define UPLOAD_INTERVAL_MS  5000   // 数据上报间隔（毫秒）
#define SENSOR_INTERVAL_MS  2000   // 传感器采集间隔（毫秒）

/* ========== 板载 LED ========== */
#define LED_PIN  LED_BUILTIN       // Arduino Nano ESP32 板载 LED (GPIO13)

/* ========== 全局对象 ========== */
WaterQualitySensor waterSensor;
WiFiClient espClient;
PubSubClient client(espClient);

/* ========== 全局变量 ========== */
unsigned long lastUploadTime = 0;    // 上次上报时间
unsigned long lastSensorTime = 0;    // 上次采集时间
bool mqttConnected = false;          // MQTT 连接状态

/* ========== 传感器数据缓存 ========== */
struct SensorData {
    float ph;              // pH 值
    float tds;             // TDS (ppm)
    int   turbidity;       // 浊度 (ADC 原始值)
    float ammonia;         // 氨氮 (mg/L)
    float temperature;     // 水温 (℃)
    float depth;           // 深度 (m)
    float depthPressure;   // 深度传感器压力 (mbar)
    float depthTemp;       // 深度传感器内部温度 (℃)
    bool  isNormal;        // 水质是否正常
    bool  depthSensorOk;   // 深度传感器是否在线
};

SensorData sensorData;

/* ========== WiFi 初始化 ========== */
void wifiInit()
{
    Serial.println("[WiFi] 正在连接...");
    WiFiClass::mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWD);

    int retryCount = 0;
    while (WiFiClass::status() != WL_CONNECTED)
    {
        delay(500);
        digitalWrite(LED_PIN, !digitalRead(LED_PIN));  // WiFi连接中 LED 快闪
        retryCount++;
        Serial.print(".");
        if (retryCount > 30) {
            Serial.println("\n[WiFi] 连接超时，重启设备...");
            ESP.restart();
        }
    }

    Serial.println();
    Serial.print("[WiFi] 连接成功，IP地址: ");
    Serial.println(WiFi.localIP());

    // 连接 WiFi 之后，设置 MQTT 服务器
    client.setServer(MQTT_SERVER, MQTT_PORT);
    // 设置心跳间隔为60秒（华为云IoTDA要求≥30秒，PubSubClient默认15秒太短会导致state 2）
    client.setKeepAlive(60);
}

/* ========== MQTT 连接维持 ========== */
void mqttCheckConnect()
{
    if (client.connected()) {
        mqttConnected = true;
        return;
    }

    Serial.println("[MQTT] 正在连接华为云 IoTDA...");
    mqttConnected = false;

    // 打印连接诊断信息
    Serial.print("[MQTT] Server: ");
    Serial.print(MQTT_SERVER);
    Serial.print(":");
    Serial.println(MQTT_PORT);
    Serial.print("[MQTT] ClientID: ");
    Serial.println(CLIENT_ID);
    Serial.print("[MQTT] Username: ");
    Serial.println(MQTT_USERNAME);

    // 尝试连接
    if (client.connect(CLIENT_ID, MQTT_USERNAME, MQTT_PASSWD)) {
        mqttConnected = true;
        Serial.println("[MQTT] 连接成功！");
    } else {
        int state = client.state();
        Serial.print("[MQTT] 连接失败，状态码: ");
        Serial.println(state);
        // 状态码含义说明
        switch (state) {
            case -4: Serial.println("[MQTT] 原因: 连接超时，检查网络或服务器地址"); break;
            case -3: Serial.println("[MQTT] 原因: 连接断开"); break;
            case -2: Serial.println("[MQTT] 原因: 连接失败"); break;
            case  2: Serial.println("[MQTT] 原因: ClientID被拒绝 或 心跳间隔不满足平台要求(需≥30秒)"); break;
            case  4: Serial.println("[MQTT] 原因: 用户名或密码错误，请重新生成MQTT密码"); break;
            case  5: Serial.println("[MQTT] 原因: 未授权，设备可能被冻结"); break;
            default: Serial.println("[MQTT] 原因: 未知错误"); break;
        }
    }
}

/* ========== MQTT 消息回调（接收平台下发命令） ========== */
void mqttCallback(char* topic, byte* payload, unsigned int length)
{
    Serial.print("[MQTT] 收到消息，Topic: ");
    Serial.println(topic);

    char msg[length + 1];
    memcpy(msg, payload, length);
    msg[length] = '\0';
    Serial.print("[MQTT] 消息内容: ");
    Serial.println(msg);

    // TODO: 可在此处解析平台下发的属性设置或命令，执行相应操作
}

/* ========== 传感器数据采集 ========== */
void readSensorData()
{
    sensorData.ph           = waterSensor.readPH();
    sensorData.tds          = waterSensor.readTDS();
    sensorData.turbidity    = waterSensor.readTurbidity();
    sensorData.ammonia      = waterSensor.readAmmoniaNitrogen();
    sensorData.temperature  = waterSensor.readTemperature();
    sensorData.depth        = waterSensor.readDepth();
    sensorData.depthPressure    = waterSensor.readDepthPressure();
    sensorData.depthTemp        = waterSensor.readDepthTemperature();
    sensorData.depthSensorOk    = waterSensor.isDepthSensorConnected();

    // 水质状态判断
    sensorData.isNormal = waterSensor.isWaterQualityNormal(
        sensorData.ph, sensorData.tds, sensorData.ammonia, sensorData.depth);

    // 报警控制
    if (!sensorData.isNormal) {
        waterSensor.triggerAlarm();
    } else {
        waterSensor.stopAlarm();
    }
}

/* ========== 数据上报到华为云 ========== */
void mqttIntervalPost()
{
    if (!mqttConnected) {
        Serial.println("[MQTT] 未连接，跳过上报");
        return;
    }

    // 确认连接状态
    if (!client.connected()) {
        Serial.println("[MQTT] client.connected()=false，重新连接");
        mqttConnected = false;
        return;
    }

    // 构建属性 JSON —— 属性名必须与华为云 IoTDA 物模型完全一致
    char properties[384];
    snprintf(properties, sizeof(properties),
        "{\"pH\":%.2f,\"TDS\":%.1f,\"turbidity\":%d,\"ammoniaNitrogen\":%.2f,\"waterTemp\":%.1f,\"depth\":%.2f,\"depthPressure\":%.1f,\"depthSensorTemp\":%.1f,\"waterQualityNormal\":%s,\"depthSensorOnline\":%s}",
        sensorData.ph,
        sensorData.tds,
        sensorData.turbidity,
        sensorData.ammonia,
        sensorData.temperature,
        sensorData.depth,
        sensorData.depthPressure,
        sensorData.depthTemp,
        sensorData.isNormal ? "true" : "false",
        sensorData.depthSensorOk ? "true" : "false",
        0.0f   // 溶解氧传感器暂未接入，上报占位值 0.0
    );

    // 组装完整 Payload
    char jsonBuf[384];
    snprintf(jsonBuf, sizeof(jsonBuf), ALINK_BODY_FORMAT, properties);

    // 发布消息
    boolean publishResult = client.publish(ALINK_TOPIC_PROP_POST, jsonBuf);

    // 串口调试输出
    Serial.println("========== MQTT 数据上报 ==========");
    Serial.print("  pH值:       "); Serial.println(sensorData.ph, 2);
    Serial.print("  TDS:        "); Serial.println(sensorData.tds, 1);
    Serial.print("  浊度(ADC):  "); Serial.println(sensorData.turbidity);
    Serial.print("  氨氮:       "); Serial.println(sensorData.ammonia, 2);
    Serial.print("  水温:       "); Serial.println(sensorData.temperature, 1);
    Serial.print("  深度:       "); Serial.println(sensorData.depth, 2);
    Serial.print("  深度压力:   "); Serial.println(sensorData.depthPressure, 1);
    Serial.print("  深度温度:   "); Serial.println(sensorData.depthTemp, 1);
    Serial.print("  水质状态:   "); Serial.println(sensorData.isNormal ? "正常" : "超标报警");
    Serial.print("  深度传感器: "); Serial.println(sensorData.depthSensorOk ? "在线" : "离线");
    Serial.print("  上报结果:   "); Serial.println(publishResult ? "成功" : "失败");
    Serial.println("===================================");
}

/* ========== 串口数据输出（表格格式） ========== */
void serialPrintData()
{
    Serial.print(sensorData.ph, 2);
    Serial.print("\t");
    Serial.print(sensorData.tds, 1);
    Serial.print("\t\t");
    Serial.print(sensorData.turbidity);
    Serial.print("\t\t");
    Serial.print(sensorData.ammonia, 2);
    Serial.print("\t\t");
    Serial.print(sensorData.temperature, 1);
    Serial.print("\t\t");
    Serial.print(sensorData.depth, 2);
    Serial.print("\t");
    Serial.println(sensorData.isNormal ? "正常" : "超标报警");
}

/* ========== setup ========== */
void setup() {
    // 串口初始化
    Serial.begin(115200);
    while (!Serial) {
        delay(10);
    }

    // 初始化传感器
    waterSensor.begin();

    // 连接 WiFi 和 MQTT
    wifiInit();

    // 设置 MQTT 消息回调
    client.setCallback(mqttCallback);

    // 初始连接 MQTT
    mqttCheckConnect();

    // 输出表头
    Serial.println("水质监测系统初始化完成");
    Serial.println("pH值\tTDS值(ppm)\t浊度(ADC)\t氨氮(mg/L)\t水温(℃)\t深度(m)\t状态");
}

/* ========== loop ========== */
void loop() {
    unsigned long now = millis();

    // 传感器数据采集（每 SENSOR_INTERVAL_MS 毫秒）
    if (now - lastSensorTime >= SENSOR_INTERVAL_MS) {
        lastSensorTime = now;

        waterSensor.enableSensor();
        readSensorData();
        serialPrintData();
    }

    // MQTT 连接维持（持续检查）
    mqttCheckConnect();

    // 数据上报（每 UPLOAD_INTERVAL_MS 毫秒）
    if (now - lastUploadTime >= UPLOAD_INTERVAL_MS) {
        lastUploadTime = now;
        mqttIntervalPost();
    }

    // 处理 MQTT 消息
    client.loop();
}
