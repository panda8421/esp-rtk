#ifndef _WIFI_MANAGER_H_
#define _WIFI_MANAGER_H_
#include "esp_err.h"
#include "esp_wifi.h"

typedef enum
{
    WIFI_STATE_CONNECTED,
    WIFI_STATE_DISCONNECTED,
}WIFI_STATE;

//wifi状态变化回调函数
typedef void(*p_wifi_state_callback)(WIFI_STATE state);
typedef void(*p_wifi_scan_callback)(int num,wifi_ap_record_t *ap_records);
/** 初始化wifi，默认进入STA模式
 * @param f wifi状态变化回调函数
 * @return 无 
*/
void wifi_manager_init(p_wifi_state_callback f);

/** 连接wifi
 * @param ssid
 * @param password
 * @return 成功/失败
*/
esp_err_t wifi_manager_connect(const char* ssid,const char* password);

esp_err_t wifi_manager_ap(void);  //进入AP模式

esp_err_t wifi_manager_scan(p_wifi_scan_callback f);  //扫描周围的AP热点

#endif
