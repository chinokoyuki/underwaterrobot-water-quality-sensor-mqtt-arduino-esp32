#include "WaterQualitySensor.h"
#include <PubSubClient.h>
#include <WiFi.h>

#define deviceId         "69632991c00ccb6d4b32b204_ABX00083_CN_1"
#define deviceSecret     "Takanashi0721"


/* 连接WIFI SSID和密码 */
#define WIFI_SSID         "HUAWEI Pura 70 Pro"
#define WIFI_PASSWD       "200509240721"


/* 线上环境域名和端口号，不需要改 */

//华为云地址
#define MQTT_SERVER       "cbd1b71b46.st1.iotda-device.cn-south-1.myhuaweicloud.com"
#define MQTT_PORT         1883
#define MQTT_USERNAME      "69632991c00ccb6d4b32b204_ABX00083_CN_1"


//硬件的ID和密码
#define CLIENT_ID         "69632991c00ccb6d4b32b204_ABX00083_CN_1_0_0_2026011206"
#define MQTT_PASSWD       "8c834a00f2633ce36a07455f812ef84a3171955848ceda2534b899f8b9736e1e"


//硬件发送消息的格式
#define ALINK_BODY_FORMAT         "{\"services\":[{\"service_id\":\"TEST\",\"properties\":%s}]}"
#define ALINK_TOPIC_PROP_POST     "$oc/devices/{" deviceId"/sys/properties/report"

WaterQualitySensor waterSensor;

bool ledState = false;

unsigned long lastMs = 0;

// 用于连接WiFi
 WiFiClient espClient;
// 用于连接华为的mqtt服务器
 PubSubClient  client(espClient);


//储存数据
float data = 0;

//连接wifi
void wifiInit()
{
    WiFiClass::mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWD);
    while (WiFiClass::status() != WL_CONNECTED)
    {
        delay(1000);
        Serial.println("WiFi not Connect");
    }
    Serial.println("WiFi is Connected");
    client.setServer(MQTT_SERVER, MQTT_PORT);   /* 连接WiFi之后，连接MQTT服务器 */
}

//保持mqtt连接
void mqttCheckConnect()
{
    while (!client.connected())
    {
        client.connect(CLIENT_ID, MQTT_USERNAME, MQTT_PASSWD);
    }
}

//上报数据到华为云
void mqttIntervalPost()
{
  // 生成0~1024之间步长为0.01的随机值
  long randomInt = random(0, 102401);  // 生成0~102400的整数
  float data = randomInt / 100.0;      // 转换为步长0.01的浮点值（0.00~1024.00）

  char properties[64];  // 扩容以容纳新增的LED属性
  char jsonBuf[128];

  // 上报数据：拼接包含number和ledStatus的合规JSON
  sprintf(properties, "{\"Number\":%.2f,\"LED_Power\":%s}", data, ledState ? "true" : "false");
  sprintf(jsonBuf, ALINK_BODY_FORMAT, properties);

  // 启用MQTT实际上报，获取上报结果
  boolean publishResult = client.publish(ALINK_TOPIC_PROP_POST, jsonBuf);

  // 串口打印调试信息
  Serial.println("------------------------");
  Serial.print("number = ");
  Serial.println(data);
  Serial.print("LED状态 = ");
  Serial.println(ledState ? "开启(true)" : "关闭(false)");
  Serial.print("MQTT上报结果:");
  Serial.println(publishResult ? "成功" : "失败");
}


void setup() {
    // COM 115200
    Serial.begin(115200);
    while (!Serial) {
        delay(10); // standby
    }
    // 初始化传感器
    waterSensor.begin();
    // 输出数据检查
    Serial.println("水质监测系统初始化完成");
    Serial.println("pH值\tTDS值(ppm)\t浊度(ADC)\t氨氮(mg/L)\t水温(℃)\t深度(m)\t状态");
}

void loop() {
    waterSensor.enableSensor();

    // 读取数据
    float phValue = waterSensor.readPH();
    float tdsValue = waterSensor.readTDS();
    int turbidityValue = waterSensor.readTurbidity();
    float ammoniaValue = waterSensor.readAmmoniaNitrogen();
    float tempValue = waterSensor.readTemperature();
    float depthValue = waterSensor.readDepth();  // 新增：深度传感器

    // 水质状态判断与报警（新增深度参数）
    bool isNormal = waterSensor.isWaterQualityNormal(phValue, tdsValue, ammoniaValue, depthValue);
    if (!isNormal) {
        waterSensor.triggerAlarm(); // 超标触发报警
    } else {
        waterSensor.stopAlarm(); // 正常关闭报警
    }

    // 串口输出数据（新增深度列）
    Serial.print(phValue, 2);
    Serial.print("\t");
    Serial.print(tdsValue, 1);
    Serial.print("\t\t");
    Serial.print(turbidityValue);
    Serial.print("\t\t");
    Serial.print(ammoniaValue, 2);
    Serial.print("\t\t");
    Serial.print(tempValue, 1);
    Serial.print("\t\t");
    Serial.print(depthValue, 2);
    Serial.print("\t");
    Serial.println(isNormal ? "正常" : "超标报警");

    //保持设备在线
    if (millis() - lastMs >= 5000)
    {
        lastMs = millis();
       mqttCheckConnect();
    }


    // 上报
    mqttIntervalPost();

    client.loop();
    delay(2000);
}