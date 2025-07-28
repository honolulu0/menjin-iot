#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
// 看门狗
#include <esp_task_wdt.h>
#include "esp_system.h"


// 设备名
String deviceMAC;

// 串口接收门禁的全局常量
#define EXPECTED_LENGTH 9
uint8_t data[EXPECTED_LENGTH];


// 定时器
esp_timer_handle_t ledTimer;

// mqtt定时器句柄
esp_timer_handle_t heartbeatTimer;

// mqtt定义心跳间隔（单位：微秒）
#define HEARTBEAT_INTERVAL_US 120000000  // 例如，120秒 = 120,000,000微秒

// 函数声明
int receiveData(uint8_t *buffer, int length, unsigned long timeout);
String processCardData(uint8_t *data);
String extractCardUID(uint8_t *cardData, int length);

void initReadCardSerial();
bool isReadCardSerialAvailable();  // 新增


void autoCloseCallback(void *arg);
void initWatchdog();
String getDeviceMAC();
void flushSerial(HardwareSerial &serial);
bool atomicWriteUIDs();  // 新增原子写入函数
void initUIDs();         // 优化后的初始化函数

String generateUIDFromIndex(uint32_t index);  // 新增UID生成函数

//LED指示灯控制
void toggleLED(void *arg);
void initLEDControl();

// 门禁继电器控制
void initRelayControl();
void openDoor();
void closeDoor();

void addCard(String uid);
void removeCard(String uid);
void clearAllCards();
void checkCard(String uid);
void listCards();

// MQTT 相关函数声明
void initMQTT(const String &deviceMAC);
bool mqtt_begin(const String &clientID);
void mqtt_sendHeartbeat(void *arg);
void mqtt_handleIncomingMessages();
void mqtt_sendCard(const String &deviceMAC, const String &cardUID);



void setup() {
  // 初始化看门狗
  initWatchdog();

  // 初始化引脚

  initLEDControl();
  initRelayControl();
  initReadCardSerial();

  // 配置 LED 闪烁定时器
  esp_timer_create_args_t ledTimerArgs = {
    .callback = &toggleLED,
    .name = "led_toggle_timer"
  };
  esp_timer_create(&ledTimerArgs, &ledTimer);
  esp_timer_start_periodic(ledTimer, 1000000);  // 每1秒触发一次（单位：微秒）

  // 初始化串口
  Serial.begin(115200);  // 用于调试输出

  // 获取并存储设备的 MAC 地址
  deviceMAC = getDeviceMAC();
  Serial.println("Device MAC: " + deviceMAC);
  initMQTT(deviceMAC);
  // 初始化文件系统
  if (!initLittleFS()) {
    Serial.println("初始化失败！");
    // ESP.restart();
  }

  // 获取 LittleFS 可用空间
  uint64_t totalBytes = LittleFS.totalBytes();  // 总空间
  uint64_t usedBytes = LittleFS.usedBytes();    // 已用空间
  uint64_t freeBytes = totalBytes - usedBytes;  // 剩余空间

  // 输出空间信息
  Serial.print("总空间: ");
  Serial.print(totalBytes);
  Serial.println(" 字节");

  Serial.print("已用空间: ");
  Serial.print(usedBytes);
  Serial.println(" 字节");

  Serial.print("剩余空间: ");
  Serial.print(freeBytes);
  Serial.println(" 字节");

  // 初始化 UID 数据结构
  initUIDs();

  // 初始化 MQTT 模块
  String clientID = "MenJinClient_" + deviceMAC;
  if (mqtt_begin(clientID)) {
    Serial.println("MQTT模块初始化成功！");
  } else {
    Serial.println("MQTT模块初始化失败！");
  }

  // 定时任务，心跳检测mqtt健康情况，如果不在线就重启mqtt模块
  esp_timer_create_args_t heartbeatTimerArgs = {
    .callback = &mqtt_sendHeartbeat,
    .name = "heartbeat_timer"
  };

  esp_timer_create(&heartbeatTimerArgs, &heartbeatTimer);
  esp_timer_start_periodic(heartbeatTimer, HEARTBEAT_INTERVAL_US);
  // 启动完成，关闭闪烁灯定时器
  esp_timer_stop(ledTimer);
  // 复位灯为低电平，防止定时器停止时是高电平
  turnLEDOff();
}

void loop() {
  // 喂狗，防止触发复位
  esp_task_wdt_reset();
  // Serial.println("喂狗");
  // 处理来自4G模块的 MQTT 消息
  mqtt_handleIncomingMessages();

  // 处理门禁卡模块数据
  if (isReadCardSerialAvailable()) {
    Serial.println("收到刷卡消息");
    int receivedLength = receiveData(data, EXPECTED_LENGTH, 500);  // 接收数据，超时 500ms

    if (receivedLength == EXPECTED_LENGTH) {
      String cardUID = processCardData(data);  // 调用函数并获取卡号
      if (cardUID != "") {
        //用于记录本地是否已经开门的标志
        int localOpened = 0;
          //本地开门。和本地卡号相符合
          if (isCardExist(cardUID)) {
          Serial.println("本地开门");
          openDoor();
          localOpened = 1;
        }
        // 远程开门，发送刷卡消息到云上，如果有权限则会返回开门信息
        //此记录会作为消息记录同时保存在线上
        mqtt_sendCard(deviceMAC, cardUID, localOpened);
      }
    }
  }

  delay(100);  // 小延时
}
