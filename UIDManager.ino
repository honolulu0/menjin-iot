
// UID 配置信息
#define UID_LENGTH 8
#define UID_COUNT 10000
#define FILE_PATH "/uids.bin"

// 哈希表大小
#define HASH_SIZE (UID_COUNT / 8)
uint8_t uidHash[HASH_SIZE];



// 实现

// 原子写入 UID 数据到文件系统
bool atomicWriteUIDs() {
  const char *tmpPath = "/uids_tmp.bin";
  
  // 打开临时文件进行写入
  File tmpFile = LittleFS.open(tmpPath, "w");
  if (!tmpFile) {
    Serial.println("无法打开临时 UID 文件进行写入！");
    return false;
  }

  // 写入 uidHash 数据
  size_t bytesWritten = tmpFile.write(uidHash, sizeof(uidHash));
  tmpFile.close();

  if (bytesWritten != sizeof(uidHash)) {
    Serial.println("写入临时 UID 文件失败！");
    LittleFS.remove(tmpPath); // 删除损坏的临时文件
    return false;
  }

  // 重命名临时文件为主文件
  if (!LittleFS.rename(tmpPath, FILE_PATH)) {
    Serial.println("无法重命名临时 UID 文件为主文件！");
    LittleFS.remove(tmpPath); // 删除临时文件以防止下次恢复
    return false;
  }

  Serial.println("UID 数据已原子性保存到主文件");
  return true;
}

// 初始化 UID 数据结构，支持从临时文件恢复
void initUIDs() {
  if (LittleFS.exists(FILE_PATH)) {
    File file = LittleFS.open(FILE_PATH, "r");
    if (file) {
      size_t bytesRead = file.read(uidHash, sizeof(uidHash));
      file.close();
      if (bytesRead == sizeof(uidHash)) {
        Serial.println("UID 数据从主文件加载成功");
        return;
      } else {
        Serial.println("主 UID 文件读取不完整，尝试从临时文件恢复...");
      }
    } else {
      Serial.println("无法打开主 UID 文件，尝试从临时文件恢复...");
    }
  }

  // 如果主文件不存在或无法读取，尝试从临时文件恢复
  if (LittleFS.exists("/uids_tmp.bin")) {
    File tmpFile = LittleFS.open("/uids_tmp.bin", "r");
    if (tmpFile) {
      size_t bytesRead = tmpFile.read(uidHash, sizeof(uidHash));
      tmpFile.close();
      if (bytesRead == sizeof(uidHash)) {
        // 将临时文件重命名为主文件
        LittleFS.rename("/uids_tmp.bin", FILE_PATH);
        Serial.println("UID 数据从临时文件恢复成功");
        return;
      } else {
        Serial.println("临时 UID 文件读取不完整");
      }
    } else {
      Serial.println("无法打开临时 UID 文件");
    }
  }

  // 如果都不存在或读取失败，初始化为空
  Serial.println("UID 文件不存在或读取失败，初始化为空");
  memset(uidHash, 0, sizeof(uidHash));
}

// 生成 UID 从索引
String generateUIDFromIndex(uint32_t index) {
  String uid = "";
  for (int i = 0; i < UID_LENGTH; i++) {
    uint8_t nibble = (index >> ((UID_LENGTH - 1 - i) * 4)) & 0xF;
    uid += String(nibble, HEX);
  }
  uid.toUpperCase();
  return uid;
}

// 计算 UID 的哈希索引
uint32_t getUIDIndex(String uid) {
  uint32_t index = 0;
  for (int i = 0; i < UID_LENGTH; i++) {
    index = (index << 4) | hexToInt(uid.charAt(i));
  }
  return index % UID_COUNT;
}

// 将字符串的十六进制字符转换为整数
uint8_t hexToInt(char hex) {
  if (hex >= '0' && hex <= '9') return hex - '0';
  if (hex >= 'A' && hex <= 'F') return hex - 'A' + 10;
  if (hex >= 'a' && hex <= 'f') return hex - 'a' + 10;
  return 0;
}

// 检查 UID 是否存在
bool isCardExist(String uid) {
  uint32_t index = getUIDIndex(uid);
  if (index >= UID_COUNT) return false; // 防止越界

  uint8_t byteIndex = index / 8;
  uint8_t bitIndex = index % 8;
  return (uidHash[byteIndex] & (1 << bitIndex)) != 0;
}

// 添加卡片
void addCard(String uid) {
  if (uid.length() != UID_LENGTH) {
    Serial.println("卡片 UID 长度错误，应为 8 个字符（4 字节）");
    StaticJsonDocument<200> doc;
    doc["msg"] = uid + "|卡片错误";
    doc["mac"] = deviceMAC;
    char jsonBuffer[128];
    serializeJson(doc, jsonBuffer);
    publishWithRetry(TOPIC_CARSTATUS, jsonBuffer);
    return;
  }

  // 检查卡片是否已存在
  if (isCardExist(uid)) {
    Serial.println("已存在");
    StaticJsonDocument<200> doc;
    doc["msg"] = uid + "|已存在";
    doc["mac"] = deviceMAC;
    char jsonBuffer[128];
    serializeJson(doc, jsonBuffer);
    publishWithRetry(TOPIC_CARSTATUS, jsonBuffer);
    return;
  }

  uint32_t index = getUIDIndex(uid);
  uint8_t byteIndex = index / 8;
  uint8_t bitIndex = index % 8;

  // 设置位图中的位为 1，表示该 UID 存在
  uidHash[byteIndex] |= (1 << bitIndex);

  // 原子性保存更新后的数据到文件
  if (atomicWriteUIDs()) {
    Serial.println("已添加并成功保存到文件系统");
    StaticJsonDocument<200> doc;
    doc["msg"] = uid + "|已添加";
    doc["mac"] = deviceMAC;
    char jsonBuffer[128];
    serializeJson(doc, jsonBuffer);
    publishWithRetry(TOPIC_CARSTATUS, jsonBuffer);
  } else {
    Serial.println("添加卡片时写入失败");
    StaticJsonDocument<200> doc;
    doc["msg"] = uid + "|添加卡片失败";
    doc["mac"] = deviceMAC;
    char jsonBuffer[128];
    serializeJson(doc, jsonBuffer);
    publishWithRetry(TOPIC_CARSTATUS, jsonBuffer);
  }
}

// 删除卡片
void removeCard(String uid) {
  if (uid.length() != UID_LENGTH) {
    Serial.println("卡片 UID 长度错误，应为 8 个字符（4 字节）");
    StaticJsonDocument<200> doc;
    doc["msg"] = uid + "|卡片错误";
    doc["mac"] = deviceMAC;
    char jsonBuffer[128];
    serializeJson(doc, jsonBuffer);
    publishWithRetry(TOPIC_CARSTATUS, jsonBuffer);
    return;
  }

  if (!isCardExist(uid)) {
    Serial.println("不存在");
    StaticJsonDocument<200> doc;
    doc["msg"] = uid + "|不存在";
    doc["mac"] = deviceMAC;
    char jsonBuffer[128];
    serializeJson(doc, jsonBuffer);
    publishWithRetry(TOPIC_CARSTATUS, jsonBuffer);
    return;
  }

  uint32_t index = getUIDIndex(uid);
  uint8_t byteIndex = index / 8;
  uint8_t bitIndex = index % 8;

  // 清除位图中的位，表示该 UID 被删除
  uidHash[byteIndex] &= ~(1 << bitIndex);

  // 原子性保存更新后的数据到文件
  if (atomicWriteUIDs()) {
    Serial.println("已删除并成功保存到文件系统");
    StaticJsonDocument<200> doc;
    doc["msg"] = uid + "|已删除";
    doc["mac"] = deviceMAC;
    char jsonBuffer[128];
    serializeJson(doc, jsonBuffer);
    publishWithRetry(TOPIC_CARSTATUS, jsonBuffer);
  } else {
    Serial.println("删除卡片时写入失败");
    StaticJsonDocument<200> doc;
    doc["msg"] = uid + "|删除卡片失败";
    doc["mac"] = deviceMAC;
    char jsonBuffer[128];
    serializeJson(doc, jsonBuffer);
    publishWithRetry(TOPIC_CARSTATUS, jsonBuffer);
  }
}

// 清空所有卡片
void clearAllCards() {
  // 清空 UID 数据
  memset(uidHash, 0, sizeof(uidHash));

  // 原子性保存空的哈希表（即所有卡片都已删除）
  if (atomicWriteUIDs()) {
    Serial.println("所有卡片已删除并成功保存到文件系统");
    StaticJsonDocument<200> doc;
    doc["msg"] = "所有卡片已删除";
    doc["mac"] = deviceMAC;
    char jsonBuffer[128];
    serializeJson(doc, jsonBuffer);
    publishWithRetry(TOPIC_CARSTATUS, jsonBuffer);
  } else {
    Serial.println("清空卡片时写入失败");
    StaticJsonDocument<200> doc;
    doc["msg"] = "清空卡片失败";
    doc["mac"] = deviceMAC;
    char jsonBuffer[128];
    serializeJson(doc, jsonBuffer);
    publishWithRetry(TOPIC_CARSTATUS, jsonBuffer);
  }
}

// 检查卡片并发布状态
void checkCard(String uid) {
  if (uid.length() != UID_LENGTH) {
    Serial.println("卡片 UID 长度错误，应为 8 个字符（4 字节）");
    StaticJsonDocument<200> doc;
    doc["msg"] = uid + "|卡片错误";
    doc["mac"] = deviceMAC;
    char jsonBuffer[128];
    serializeJson(doc, jsonBuffer);
    publishWithRetry(TOPIC_CARSTATUS, jsonBuffer);
    return;
  }

  if (isCardExist(uid)) {
    Serial.println("正常");
    StaticJsonDocument<200> doc;
    doc["msg"] = uid + "|正常";
    doc["mac"] = deviceMAC;
    char jsonBuffer[128];
    serializeJson(doc, jsonBuffer);
    publishWithRetry(TOPIC_CARSTATUS, jsonBuffer);
  } else {
    Serial.println("不存在");
    StaticJsonDocument<200> doc;
    doc["msg"] = uid + "|不存在";
    doc["mac"] = deviceMAC;
    char jsonBuffer[128];
    serializeJson(doc, jsonBuffer);
    publishWithRetry(TOPIC_CARSTATUS, jsonBuffer);
  }
}

// 列出所有存储的 UID（从内存中读取）
void listCards() {
  Serial.println("列出所有存储的 UID:");
  for (uint32_t i = 0; i < UID_COUNT; i++) {
    uint8_t byteIndex = i / 8;
    uint8_t bitIndex = i % 8;
    if (uidHash[byteIndex] & (1 << bitIndex)) {
      String uid = generateUIDFromIndex(i);
      Serial.println(uid);
    }
  }
}
