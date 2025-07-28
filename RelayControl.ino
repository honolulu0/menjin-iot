#define RELAY_PIN 16


// 初始化 Relay 控制
void initRelayControl() {
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
}


// 打开 Relay
void turnRelayOn() {
  digitalWrite(RELAY_PIN, HIGH);
  ledState = true;
}

// 关闭 Relay
void turnRelayOff() {
  digitalWrite(RELAY_PIN, LOW);
  ledState = false;
}



// 门控制相关函数
void openDoor() {
  turnRelayOn();
  turnLEDOn();
  Serial.println("门已开");
  delay(1000);  // 延迟一秒关闭继电器
  closeDoor();
}

void closeDoor() {
  turnRelayOff();
  turnLEDOff();
  Serial.println("已关门");
}

// 自动关门定时器回调
void autoCloseCallback(void *arg) {
  closeDoor();
  StaticJsonDocument<200> doc;
  doc["msg"] = "Door closed (auto)";
  doc["mac"] = deviceMAC;
  char jsonBuffer[128];
  serializeJson(doc, jsonBuffer);
  publishWithRetry(TOPIC_DOORSTATUS, jsonBuffer);
}
