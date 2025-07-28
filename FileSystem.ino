
// 文件路径
#define FILE_PATH "/uids.bin"


// 实现

// 初始化 LittleFS
bool initLittleFS() {
  if (!LittleFS.begin()) {
    Serial.println("LittleFS 初始化失败！正在进行格式化...");
    // 尝试格式化文件系统
    LittleFS.format();
    // 再次初始化
    if (!LittleFS.begin()) {
      Serial.println("格式化后仍然初始化失败！");
      return false;
    }
    Serial.println("LittleFS 格式化并初始化成功！");
    return true;
  }
  Serial.println("LittleFS 初始化成功！");
  return true;
}
