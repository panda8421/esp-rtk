#include <stdio.h>
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_smartconfig.h"
#include <string.h>

#define TAG   "smartconfig"

/**
 * Wi-Fi事件处理函数
 * 用于响应Wi-Fi连接状态变化、IP地址获取等事件
 * @param event_handler_arg: 事件处理参数（未使用）
 * @param event_base: 事件类型（如WIFI_EVENT、IP_EVENT）
 * @param event_id: 具体事件ID（如连接成功、断开连接）
 * @param event_data: 事件相关数据（未使用）
 */
void wifi_event_handle(void* event_handler_arg,esp_event_base_t event_base,int32_t event_id,void* event_data)
{
    if(event_base == WIFI_EVENT)// 处理Wi-Fi相关事件（WIFI_EVENT类型）
    {
        switch(event_id)
        {
            case WIFI_EVENT_STA_START: //事件1：STA模式启动完成（Wi-Fi已准备好连接）
                esp_wifi_connect();// 调用连接函数，尝试连接到指定Wi-Fi
                break;
            case WIFI_EVENT_STA_CONNECTED:// 事件2：STA成功连接到Wi-Fi热点
                ESP_LOGI(TAG,"esp32 connected to ap!");
                break;
            case WIFI_EVENT_STA_DISCONNECTED:// 事件3：STA与Wi-Fi热点断开连接
                esp_wifi_connect(); // 自动重试连接
                ESP_LOGI(TAG,"esp32 connect the ap faild! retry!");
                break;
            default:break;

        }
    }
    else if(event_base == IP_EVENT)//处理IP相关事件（IP_EVENT类型)
    {
        switch(event_id)
        {
            case IP_EVENT_STA_GOT_IP://事件：STA成功获取IP地址（此时可正常联网）
                ESP_LOGI(TAG,"esp32 get ip address");
                break;
        }
    }
    else if(event_base == SC_EVENT)// 智能配网（SmartConfig）相关事件（SC_EVENT 是智能配网事件的统一类型标识）
    {
        switch (event_id)
        {
        case SC_EVENT_SCAN_DONE:// 事件1：智能配网的“Wi-Fi扫描完成”事件
            ESP_LOGI(TAG,"sc scan done");
            break;
        // 事件2：智能配网的“成功获取Wi-Fi账号（SSID）和密码（PSWD）”事件
        // （手机APP通过声波/广播发送的Wi-Fi信息，被ESP32接收并解析后触发此事件）
        case SC_EVENT_GOT_SSID_PSWD:
        {
            //将事件数据转换为“智能配网获取账号密码”的结构体类型
            // smartconfig_event_got_ssid_pswd_t 是ESP-IDF定义的结构体，专门存储配网获取的Wi-Fi信息
            smartconfig_event_got_ssid_pswd_t *evt = (smartconfig_event_got_ssid_pswd_t*)event_data;

            // 2. 初始化Wi-Fi配置结构体（用于存储要连接的Wi-Fi参数）
            wifi_config_t wifi_config = {0};// 初始化为全0，避免残留垃圾数据
            memset(&wifi_config,0,sizeof(wifi_config));// 再次清空结构体，确保安全

            // 3. 将获取到的Wi-Fi账号（SSID）和密码复制到Wi-Fi配置中
            // snprintf：安全的字符串复制，避免数组越界（参数依次为：目标数组、数组最大长度、源字符串）
            snprintf((char*)wifi_config.sta.ssid,sizeof(wifi_config.sta.ssid),"%s",(char*)evt->ssid);
            snprintf((char*)wifi_config.sta.password,sizeof(wifi_config.sta.password),"%s",(char*)evt->password);

            // 5. 设置是否绑定BSSID（BSSID是路由器的MAC地址，绑定后设备只会连接指定路由器，避免同名Wi-Fi干扰）
            // evt->bssid_set 由手机APP决定：若APP选择“指定路由器”，则为true；否则为false
            wifi_config.sta.bssid_set = evt->bssid_set;
            if(wifi_config.sta.bssid_set)
            {
                memcpy(wifi_config.sta.bssid,evt->bssid,6);// MAC地址固定6字节，直接复制
            }
            esp_wifi_disconnect();//先断开当前可能存在的Wi-Fi连接（避免旧连接影响新配置）
            esp_wifi_set_config(WIFI_IF_STA,&wifi_config);//将获取到的Wi-Fi配置（账号、密码、BSSID）应用到ESP32的STA模式
            esp_wifi_connect();//按照新配置，发起Wi-Fi连接（此时设备会连接到手机APP指定的Wi-Fi热点）
            break;
        }
        // 事件3：智能配网的“发送确认信息完成”事件
        // （ESP32成功获取Wi-Fi信息后，会向手机APP发送“已收到信息”的确认，发送完成后触发此事件）
        case SC_EVENT_SEND_ACK_DONE:
        {
            esp_smartconfig_stop();// 停止智能配网服务（配网流程已完成，释放相关资源，避免占用内存）
            break;
        }
        default:
            break;
        }
    }
}



void app_main(void)
{
   // 1. 初始化NVS闪存（用于存储Wi-Fi配置等信息，必须先初始化）
    // ESP_ERROR_CHECK：检查函数返回值，若出错则终止程序并打印错误
    ESP_ERROR_CHECK(nvs_flash_init());
    //2. 初始化网络接口（创建网络协议栈，如TCP/IP）
    ESP_ERROR_CHECK(esp_netif_init());
    // 3. 创建默认事件循环（用于处理Wi-Fi、IP等事件的消息队列）
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    // 4. 创建默认的STA模式网络接口（绑定Wi-Fi STA与TCP/IP协议栈）
    esp_netif_create_default_wifi_sta();
    // 5. 初始化Wi-Fi配置（使用默认配置，包含MAC地址、信道等基础参数）
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));// 应用Wi-Fi初始化配置

    // 6. 注册事件处理器（将事件与处理函数绑定）
    // 注册所有Wi-Fi事件（ESP_EVENT_ANY_ID）到wifi_event_handle函数
    esp_event_handler_register(WIFI_EVENT,ESP_EVENT_ANY_ID,wifi_event_handle,NULL);
    // 注册IP获取事件（IP_EVENT_STA_GOT_IP）到wifi_event_handle函数
    esp_event_handler_register(IP_EVENT,IP_EVENT_STA_GOT_IP,wifi_event_handle,NULL);
    esp_event_handler_register(SC_EVENT,ESP_EVENT_ANY_ID,wifi_event_handle,NULL);

    // 8. 设置Wi-Fi工作模式为STA（客户端模式，连接其他热点）
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    // 启动Wi-Fi（开始执行连接流程）
    ESP_ERROR_CHECK(esp_wifi_start());

    esp_smartconfig_set_type(SC_TYPE_ESPTOUCH);//设置智能配网的协议类型。
    smartconfig_start_config_t sc_cfg = SMARTCONFIG_START_CONFIG_DEFAULT();//初始化智能配网的配置参数结构体，并使用默认配置。
    esp_smartconfig_start(&sc_cfg);//根据前面配置，正式启动智能配网服务。

}
