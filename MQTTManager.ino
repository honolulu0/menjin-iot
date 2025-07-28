// MQTT引脚配置
#define MQTT_RX_PIN 26
#define MQTT_TX_PIN 27


// AT 命令超时时间（毫秒）
#define RESET_TIMEOUT 6000
#define GENERAL_TIMEOUT 6000
#define PUBLISH_TIMEOUT 2000



// MQTT 配置信息
#define MQTT_SERVER "menjin.aoaosheng.com"
#define MQTT_PORT 1883
#define MQTT_USER "weid"
#define MQTT_PASSWORD "10086"

// MQTT 相关变量
HardwareSerial mqttSerial(1);  // UART1 用于 4G MQTT 模块

// MQTT 订阅主题
const String BASE_TOPIC_ACCESSCTL = "accessctl/";
String TOPIC_ACCESSCTL;

// MQTT 发布主题
const String TOPIC_CARDSCAN = "card/scan/";
const String TOPIC_DOORSTATUS = "doorstatus/";
const String TOPIC_SYSTEMSTATUS = "systemstatus/";
const String TOPIC_CARSTATUS = "cardstatus/";
const String TOPIC_PING = "ping/";


// MQTT 失败重试相关
#define MAX_PUBLISH_FAILURES 2
// 发送失败的计数器
int publishFailureCount = 0;

void initMQTT(const String &deviceMAC) {

  // 定义完整主题
  TOPIC_ACCESSCTL = BASE_TOPIC_ACCESSCTL + deviceMAC;
  mqttSerial.begin(115200, SERIAL_8N1, MQTT_RX_PIN, MQTT_TX_PIN);  // UART1 用于 MQTT 模块
};

// MQTT 初始化函数
bool mqtt_begin(const String &clientID) {
  flushSerial(mqttSerial);  // 清空缓冲区

  // 重置设备
  if (!mqtt_sendATCommand("AT+RESET", "+NITZ:", RESET_TIMEOUT)) return false;
  delay(200);

  // 关闭回显
  flushSerial(mqttSerial);
  String qicsgpCmd = "ATE0";
  if (!mqtt_sendATCommand(qicsgpCmd, "OK", GENERAL_TIMEOUT)) return false;
  delay(100);

  // 打开网络连接
  flushSerial(mqttSerial);
  if (!mqtt_sendATCommand("AT+NETOPEN", "+NETOPEN:SUCCESS", GENERAL_TIMEOUT)) return false;
  delay(200);

  // 配置MQTT服务器
  String mqttConfigCmd = "AT+MCONFIG=\"" + clientID + "\",\"" + String(MQTT_USER) + "\",\"" + String(MQTT_PASSWORD) + "\",0,0,0,\"" + String(MQTT_PORT) + "\",\"2024\"";
  flushSerial(mqttSerial);
  if (!mqtt_sendATCommand(mqttConfigCmd, "OK", GENERAL_TIMEOUT)) return false;
  delay(100);

  // 启动MQTT连接
  String mqttStartCmd = "AT+MIPSTART=\"" + String(MQTT_SERVER) + "\"," + String(MQTT_PORT) + ",4";
  flushSerial(mqttSerial);
  if (!mqtt_sendATCommand(mqttStartCmd, "+MIPSTART: SUCCESS", GENERAL_TIMEOUT)) return false;
  delay(200);

  // 连接到MQTT服务器
  String connectCmd = "AT+MCONNECT=1,60";
  flushSerial(mqttSerial);
  if (!mqtt_sendATCommand(connectCmd, "+MCONNECT: SUCCESS", GENERAL_TIMEOUT)) return false;
  delay(200);

  // 订阅主题
  if (!mqtt_subscribe(TOPIC_ACCESSCTL, 0)) return false;

  return true;
}

// 订阅MQTT主题
bool mqtt_subscribe(const String &topic, int qos) {
  String cmd = "AT+MSUB=\"" + topic + "\"," + String(qos);
  flushSerial(mqttSerial);
  return mqtt_sendATCommand(cmd, "+MSUB: SUCCESS", GENERAL_TIMEOUT);
}

// 取消订阅MQTT主题
bool mqtt_unsubscribe(const String &topic) {
  String cmd = "AT+MUNSUB=\"" + topic + "\"";
  flushSerial(mqttSerial);
  return mqtt_sendATCommand(cmd, "+MUNSUB: SUCCESS", GENERAL_TIMEOUT);
}

// 定时任务，查询 MQTT 连接状态
void mqtt_sendHeartbeat(void *arg) {
  // String cmd = "AT+MQTTSTATU";
  String command = "AT+MPUB=\"" + TOPIC_CARDSCAN + "\",0,0,\"ping\"";
  flushSerial(mqttSerial);
  if (!publishWithRetry(TOPIC_PING, "ping")) {
    mqtt_reset();
  };
}

// 发布消息到MQTT
bool mqtt_publish(const String &topic, const String &jsonPayload, int qos = 0, bool retain = false) {
  String cmd = "AT+MPUB=\"" + topic + "\"," + String(qos) + "," + (retain ? "1" : "0") + ",\"" + jsonPayload + "\"";
  flushSerial(mqttSerial);
  return mqtt_sendATCommand(cmd, "+MPUB: SUCCESS", GENERAL_TIMEOUT);
}

// 发送 AT 命令并等待响应
bool mqtt_sendATCommand(String command, String expectedResponse, unsigned long timeout, String &responseContent) {
  responseContent = "";         // 初始化响应内容
  mqttSerial.println(command);  // 发送 AT 命令到 ESP32
  unsigned long start = millis();

  // 等待响应
  while (millis() - start < timeout) {
    // 喂狗，防止触发复位，在定时器中不喂狗
    if (esp_task_wdt_status(NULL) == ESP_OK) {
      esp_task_wdt_reset();
    }
    delay(10);  // 添加 10 毫秒的延迟，避免过于频繁地调用
    while (mqttSerial.available()) {
      char c = mqttSerial.read();
      responseContent += c;

      // 检查是否有错误响应
      if (responseContent.indexOf("ERROR") != -1) {
        Serial.println("收到命令失败: " + command);
        Serial.println("响应内容: " + responseContent);
        return false;
      }

      // 检查是否有预期的响应
      if (responseContent.indexOf(expectedResponse) != -1) {
        Serial.println("命令成功执行: " + command);
        return true;  // 命令成功执行
      }
    }
  }

  Serial.println("错误: 命令执行失败或超时: " + command);
  Serial.println("-------------");
  Serial.println(responseContent);
  Serial.println("++++++++++++++");
  Serial.println(expectedResponse);
  Serial.println("-------------");
  return false;  // 命令执行失败或超时
}

// 不带 responseContent 的重载函数
bool mqtt_sendATCommand(String command, String expectedResponse, unsigned long timeout) {
  String tempResponse;
  return mqtt_sendATCommand(command, expectedResponse, timeout, tempResponse);
}

// 处理来自4G模块的 MQTT 消息
void mqtt_handleIncomingMessages() {
  while (mqttSerial.available()) {
    String incoming = mqttSerial.readStringUntil('\n');
    incoming.trim();  // 去除首尾空白字符
    Serial.println("接收到: " + incoming);

    // 解析收到的消息
    if (incoming.startsWith("+MSUB:")) {
      // 提取主题和消息内容，忽略消息长度部分
      int firstComma = incoming.indexOf(',');
      if (firstComma == -1) {
        Serial.println("无法找到第一个逗号！");
        continue;
      }

      int secondComma = incoming.indexOf(',', firstComma + 1);
      if (secondComma == -1) {
        Serial.println("无法找到第二个逗号！");
        continue;
      }

      // 找到第二个逗号之后的第一个双引号
      int messageQuoteStart = incoming.indexOf('"', secondComma + 1);
      if (messageQuoteStart == -1) {
        Serial.println("无法找到消息的起始引号！");
        continue;
      }

      // 找到消息的结束双引号
      int messageQuoteEnd = incoming.lastIndexOf('"');
      if (messageQuoteEnd == -1 || messageQuoteEnd <= messageQuoteStart) {
        Serial.println("无法找到消息的结束引号！");
        continue;
      }

      // 提取消息内容
      String message = incoming.substring(messageQuoteStart + 1, messageQuoteEnd);

      // 提取主题
      int firstQuote = incoming.indexOf('"');
      int secondQuote = incoming.indexOf('"', firstQuote + 1);
      if (firstQuote == -1 || secondQuote == -1) {
        Serial.println("无法解析主题！");
        continue;
      }
      String topicStr = incoming.substring(firstQuote + 1, secondQuote);

      Serial.println("主题: " + topicStr + " - 消息: " + message);

      // 根据主题和消息内容执行相应操作
      if (topicStr == TOPIC_ACCESSCTL) {
        // 使用JSON格式解析命令
        StaticJsonDocument<200> doc;
        DeserializationError error = deserializeJson(doc, message);
        if (error) {
          Serial.print("JSON deserialization failed: ");
          Serial.println(error.c_str());
          continue;
        }

        const char *command = doc["command"];
        const char *parameter = doc["parameter"];

        if (strcmp(command, "led") == 0) {
          if (strcmp(parameter, "on") == 0) {
            digitalWrite(LED_PIN, HIGH);
            Serial.println("LED turned on");
          } else if (strcmp(parameter, "off") == 0) {
            digitalWrite(LED_PIN, LOW);
            Serial.println("LED turned off");
          }
        } else if (strcmp(command, "door") == 0) {
          if (strcmp(parameter, "open") == 0) {
            openDoor();
            StaticJsonDocument<200> doc;
            doc["msg"] = "Door opened";
            doc["mac"] = deviceMAC;
            char jsonBuffer[128];
            serializeJson(doc, jsonBuffer);

            publishWithRetry(TOPIC_DOORSTATUS, jsonBuffer);
          } else if (strcmp(parameter, "close") == 0) {
            closeDoor();
            StaticJsonDocument<200> doc;
            doc["msg"] = "Door closed";
            doc["mac"] = deviceMAC;
            char jsonBuffer[128];
            serializeJson(doc, jsonBuffer);

            publishWithRetry(TOPIC_DOORSTATUS, jsonBuffer);
          } else if (strcmp(parameter, "reject") == 0) {
            Serial.println("Reject command received.");
          }
        } else if (strcmp(command, "add") == 0) {
          addCard(parameter);
        } else if (strcmp(command, "del") == 0) {
          removeCard(parameter);
        } else if (strcmp(command, "delall") == 0) {
          clearAllCards();
        } else if (strcmp(command, "check") == 0) {
          checkCard(parameter);
        } else if (strcmp(command, "system") == 0) {
          if (strcmp(parameter, "restart") == 0) {
            Serial.println("Restart command received via MQTT. Restarting...");

            StaticJsonDocument<200> doc;
            doc["msg"] = "Restarting device";
            doc["mac"] = deviceMAC;

            char jsonBuffer[128];
            serializeJson(doc, jsonBuffer);

            publishWithRetry(TOPIC_SYSTEMSTATUS, jsonBuffer);
            delay(1000);  // 确保消息发送完毕
            ESP.restart();
          }
        } else if (strcmp(command, "at") == 0) {
          //如果返回不是OK，超过3秒也会返回
          //{ "command": "at","parameter": "AT+ICCID" }
          String response;
          //发送at指令给模块
          mqtt_sendATCommand(parameter, "OK", 3, response);
          response.replace(",", "，");
          StaticJsonDocument<200> doc;
          doc["msg"] = response;
          doc["mac"] = deviceMAC;
          char jsonBuffer[300];
          serializeJson(doc, jsonBuffer);
          publishWithRetry(TOPIC_SYSTEMSTATUS, jsonBuffer);
        }
      }
    }
  }
}


// 发布消息并处理失败重试逻辑
bool publishWithRetry(const String &topic, const String &jsonPayload) {
  for (int attempt = 1; attempt <= MAX_PUBLISH_FAILURES; ++attempt) {
    Serial.println("尝试 " + String(attempt) + " 发送消息。");
    if (mqtt_publish(topic, jsonPayload)) {
      // 发送成功，重置失败计数器
      publishFailureCount = 0;
      return true;
    } else {
      // 发送失败，增加失败计数器
      publishFailureCount++;
      Serial.println("尝试 " + String(attempt) + " 发送消息失败。");
    }
  }

  // 如果失败次数达到最大值，重启ESP32，暂时不重启设备了
  // if (publishFailureCount >= MAX_PUBLISH_FAILURES) {
  //   Serial.println("发布失败次数达到最大值，正在重启ESP32...");
  //   delay(1000);    // 确保所有信息都已打印
  //   ESP.restart();  // 重启ESP32
  // }

  return false;  // 发送失败
}


void mqtt_reset() {

  Serial.println("发布失败次数达到最大值，正在重启4G");
  delay(10);  // 确保所有信息都已打印

  String clientID = "MenJinClient_" + deviceMAC;
  if (mqtt_begin(clientID)) {
    Serial.println("MQTT模块重新初始化成功！");

    // 重新订阅主题
    if (mqtt_subscribe(TOPIC_ACCESSCTL, 0)) {
      Serial.println(String("重新订阅主题 '") + TOPIC_ACCESSCTL + String("' 成功！"));
    } else {
      Serial.println(String("重新订阅主题 '") + TOPIC_ACCESSCTL + String("' 失败！"));
    }
  } else {
    Serial.println("MQTT模块重新初始化失败！");
  }
}

void mqtt_sendCard(const String &deviceMAC, const String &cardUID, int localOpened) {

  // 将卡号和设备MAC封装为JSON格式
  StaticJsonDocument<200> doc;
  doc["cardUID"] = cardUID;
  doc["deviceMAC"] = deviceMAC;
  doc["localOpened"] = localOpened;
  char jsonBuffer[512];
  serializeJson(doc, jsonBuffer);
  String command = "AT+MPUB=\"" + TOPIC_CARDSCAN + "\",0,0,\"" + String(jsonBuffer) + "\"";
  Serial.println(command);
  flushSerial(mqttSerial);
  mqttSerial.println(command);  // 发送AT命令到ESP32


  // if (publishWithRetry(TOPIC_CARDSCAN, jsonBuffer)) {  // 发送JSON消息到 MQTT 主题
  //   Serial.println("Card scanned: " + String(jsonBuffer));
  //   return true;
  // } else {
  //   Serial.println("发送刷卡消息 '" + String(jsonBuffer) + "' 失败！");
  //   return false;
  // }
}
