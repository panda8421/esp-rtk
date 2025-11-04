#include <stdio.h>
#include "onenet_mqtt.h"
#include "wifi_manager.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "onenet_dm.h"

static EventGroupHandle_t wifi_ev = NULL;
#define WIFI_CONNECT_BIT   BIT0

/**
 * wifi事件回调函数
 * @param ev wifi事件
 * @return 无
 */
static void wifi_state_callback(WIFI_STATE state)
{
    if(state == WIFI_STATE_CONNECTED)
    {
        xEventGroupSetBits(wifi_ev,WIFI_CONNECT_BIT);
    }
}

void app_main(void)
{
    // 1. 初始化 NVS 闪存（非易失性存储）
    // NVS 用于保存 Wi-Fi 密码、设备配置等需要断电后保留的数据
    nvs_flash_init();
    // 2. 创建事件组（用于同步 Wi-Fi 连接状态）
    // 事件组是 FreeRTOS 提供的同步机制，这里用于标记 Wi-Fi 是否连接成功
    wifi_ev = xEventGroupCreate();
    // 3. 初始化 Wi-Fi 管理器，并注册状态回调函数
    // wifi_manager_init：初始化 Wi-Fi 相关资源（如配置 Wi-Fi 模式、注册事件处理等）
    // wifi_state_callback：自定义回调函数，用于接收 Wi-Fi 连接/断开等状态通知
    onenet_dm_init();
    wifi_manager_init(wifi_state_callback);
    // 4. 调用 Wi-Fi 管理器连接指定的 Wi-Fi 热点
    // 参数：Wi-Fi 热点名称（SSID）为 "Lymow"，密码为 "1415926535"
    wifi_manager_connect("Lymow","1415926535");//连接的WIFI用户名跟密码
    EventBits_t ev;
    while(1)
    {
        //等待WIFI连接成功，然后才开始连接onenet
        ev = xEventGroupWaitBits(wifi_ev,WIFI_CONNECT_BIT,pdTRUE,pdFALSE,pdMS_TO_TICKS(10*1000));
        if(ev & WIFI_CONNECT_BIT)
        {
            // Wi-Fi 已就绪，启动 OneNet 平台连接（如 MQTT 连接）
            onenet_start();
        }
    }
}
