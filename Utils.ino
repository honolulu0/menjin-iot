
// 获取设备MAC地址（不使用WiFi）
String getDeviceMAC() {
  uint64_t chipid = ESP.getEfuseMac();  // 获取64位的MAC地址
  String macStr = "";

  // 将64位MAC地址转换为6字节并格式化为字符串
  for (int i = 5; i >= 0; i--) {
    uint8_t byte = (chipid >> (i * 8)) & 0xFF;  // 提取每个字节
    if (byte < 16) {
      macStr += "0";  // 添加前导零
    }
    macStr += String(byte, HEX);  // 添加十六进制字符串
  }

  macStr.toUpperCase();  // 将字符串转为大写
  return macStr;
}

// 清空串口接收缓冲区
void flushSerial(HardwareSerial &serial) {
  while (serial.available()) {
    serial.read();
  }
}
