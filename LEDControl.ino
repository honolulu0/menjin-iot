// LED 引脚
#define LED_PIN 2
// LED 状态
bool ledState = false;



// 初始化 LED 控制
void initLEDControl() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
}

// 打开 LED
void turnLEDOn() {
  digitalWrite(LED_PIN, HIGH);
  ledState = true;
}

// 关闭 LED
void turnLEDOff() {
  digitalWrite(LED_PIN, LOW);
  ledState = false;
}

// LED 闪烁控制（不依赖WiFi状态）
void toggleLED(void *arg) {
  ledState = !ledState;
  digitalWrite(LED_PIN, ledState);
}
