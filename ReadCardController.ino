// 引脚配置
#define TTL_RX_PIN 25

// 串口相关
HardwareSerial readCardSerial(2);  // UART2 用于门禁刷卡模块




// 定时器句柄
extern esp_timer_handle_t autoCloseTimer;

// 外部变量
extern String deviceMAC;


// 初始化 ReadCard 控制
void initReadCardSerial() {
  readCardSerial.begin(9600, SERIAL_8N1, TTL_RX_PIN, -1);  // UART2 用于门禁刷卡模块，TX 未使用
};


bool isReadCardSerialAvailable() {
  return readCardSerial.available();  // UART2 用于门禁刷卡模块，TX 未使用
};

// 接收数据函数
int receiveData(uint8_t *buffer, int length, unsigned long timeout) {
  int index = 0;
  unsigned long startTime = millis();

  // 等待接收完整数据或超时
  while (index < length && millis() - startTime < timeout) {
    if (readCardSerial.available()) {
      buffer[index++] = readCardSerial.read();
    }
  }

  return index;
}

// 处理卡号数据函数
String processCardData(uint8_t *data) {
  // 检查起始和结束字节
  if (data[0] != 0x02 || data[8] != 0x03) {
    Serial.println("Invalid start or end bytes!");
    return "";
  }

  // 验证长度字节
  if (data[1] != EXPECTED_LENGTH) {
    Serial.println("Invalid length!");
    return "";
  }

  // 验证 BCC 校验
  uint8_t calculatedBCC = data[1] ^ data[2] ^ data[3] ^ data[4] ^ data[5] ^ data[6];
  if (data[7] != calculatedBCC) {
    Serial.print("BCC checksum invalid! Calculated: 0x");
    Serial.print(calculatedBCC, HEX);
    Serial.print(", Received: 0x");
    Serial.println(data[7], HEX);
    return "";
  }

  // 提取卡号
  String cardUID = extractCardUID(data + 3, 4);  // 从第4字节到第7字节
  cardUID.toUpperCase();                         // 转换为大写
  return cardUID;
}

// 提取卡号函数
String extractCardUID(uint8_t *cardData, int length) {
  String cardUID = "";
  for (int i = 0; i < length; i++) {
    if (cardData[i] < 0x10) {
      cardUID += "0";  // 补零，确保两位十六进制表示
    }
    cardUID += String(cardData[i], HEX);
  }
  return cardUID;
}
