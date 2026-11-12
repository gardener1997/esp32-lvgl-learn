#pragma once

#include <time.h>

// 启动 NTP 时间同步。
// 可以重复调用，内部只配置一次。
void time_service_begin();

// 检查系统时间是否已经同步。
bool time_service_is_synced();

// 获取当前北京时间。
// 尚未同步时返回 false，不会阻塞等待。
bool time_service_get_local_time(struct tm *timeinfo);
