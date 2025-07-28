#include "esp_task_wdt.h"

void initWatchdog() {
  // 创建配置结构体
  esp_task_wdt_config_t config = {};
  config.timeout_ms = 10000;  // 设置超时时间为10秒

  // 初始化任务看门狗
  esp_task_wdt_init(&config);

  // 将当前任务添加到看门狗
  esp_task_wdt_add(NULL);
}
