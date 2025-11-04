#include <stdio.h>
#include "mqtt_client.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "freertos/semphr.h"

#define MQTT_ADDRESS   "mqtt://broker-cn.emqx.io"
#define MQTT_CLIENTID  "mqttx_esp321021"
#define MQTT_USERNAME  "panda"
#define MQTT_PASSWORD  "xxyy77.."

#define MQTT_TOPIC1    "/topic/esp32_0823"       //ESP32往这个主题推送消息
#define MQTT_TOPIC2    "/topic/mqttx_0823"       //mqttx往这个主题推送消息

#define TEST_SSID "Lymow"// 要连接的Wi-Fi热点名称（SSID）
#define TEST_PASSWORD "1415926535"// 要连接的Wi-Fi密码

static esp_mqtt_client_handle_t mqtt_handle = NULL;

static SemaphoreHandle_t s_wifi_connect_sem = NULL;

#define TAG "mqtt"

void mqtt_event_callback(void* event_handle_arg,esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    // 将事件数据转换为 MQTT 事件专用结构体（方便获取事件详情）
    // esp_mqtt_event_handle_t 结构体中包含：主题、消息内容、消息长度、客户端句柄等信息
    esp_mqtt_event_handle_t data = (esp_mqtt_event_handle_t)event_data;
    // 根据不同的事件 ID，执行对应的处理逻辑
    switch (event_id)
    {
        // 事件1：MQTT 客户端成功连接到服务器
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG,"mqtt connected");
        // 连接成功后，立即订阅指定的 MQTT 主题（例如接收服务器/手机发来的指令）
         // 参数：客户端句柄、要订阅的主题（MQTT_TOPIC2）、QoS 等级（1表示确保消息至少到达一次）
        esp_mqtt_client_subscribe_single(mqtt_handle,MQTT_TOPIC2,1);
        break;
        // 事件2：MQTT 客户端与服务器断开连接
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG,"mqtt disconnected");
        break;
    // 事件3：MQTT 客户端发送消息后，收到服务器的“发布确认”（表示服务器已收到消息）
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG,"mqtt published ack");
        break;
      // 事件4：MQTT 客户端订阅主题后，收到服务器的“订阅确认”（表示订阅成功）
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG,"mqtt subcribed ack");
        break;
    // 事件5：MQTT 客户端收到服务器发送的消息（来自已订阅的主题）
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG,"topic->%s",data->topic);
        ESP_LOGI(TAG,"payload->%s",data->data);
        break;
    
    default:
        break;
    }
}

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
                xSemaphoreGive(s_wifi_connect_sem);
                break;
        }
    }
}


void mqtt_start(void)
{
    // 1. 定义并初始化MQTT客户端配置结构体（初始化为全0，避免垃圾数据）
    // esp_mqtt_client_config_t 是ESP-IDF定义的结构体，用于存储MQTT连接的所有参数
    esp_mqtt_client_config_t   mqtt_cfg = {0};
    // 2. 配置MQTT服务器的URI（统一资源标识符，包含协议和地址）
    mqtt_cfg.broker.address.uri = MQTT_ADDRESS;
    // 3. 配置MQTT服务器的端口号（默认1883为非加密端口，8883为加密端口）
    mqtt_cfg.broker.address.port = 1883;
    // 4. 配置客户端ID（用于在MQTT服务器中唯一标识当前ESP32设备，避免重复）
    mqtt_cfg.credentials.client_id = MQTT_CLIENTID;
    // 5. 配置连接MQTT服务器的用户名
    mqtt_cfg.credentials.username = MQTT_USERNAME;
    // 6. 配置连接MQTT服务器的密码（与用户名配套，用于身份验证）
    mqtt_cfg.credentials.authentication.password = MQTT_PASSWORD;
    // 7. 根据上述配置，初始化MQTT客户端，并获取客户端句柄（类似"连接ID"，后续操作需用它）
    // mqtt_handle 是全局变量，用于保存客户端句柄，方便其他函数操作MQTT连接
    mqtt_handle = esp_mqtt_client_init(&mqtt_cfg);
    // 8. 注册MQTT事件回调函数（关键步骤！用于处理连接、断开、接收消息等事件）
    // 参数说明：
    // - mqtt_handle：客户端句柄（指定为哪个客户端注册回调）
    // - ESP_EVENT_ANY_ID：监听所有MQTT相关事件（如连接成功、接收消息等）
    // - mqtt_event_callback：自定义的事件处理函数（收到事件后会自动调用）
    // - NULL：回调函数的参数（此处未使用）
    esp_mqtt_client_register_event(mqtt_handle,ESP_EVENT_ANY_ID,mqtt_event_callback,NULL);
    // 9. 启动MQTT客户端，正式发起与服务器的连接
    // 连接成功后，会触发MQTT_EVENT_CONNECTED事件，在回调函数中处理后续操作（如订阅主题）
    esp_mqtt_client_start(mqtt_handle);
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

    s_wifi_connect_sem = xSemaphoreCreateBinary();

    // 6. 注册事件处理器（将事件与处理函数绑定）
    // 注册所有Wi-Fi事件（ESP_EVENT_ANY_ID）到wifi_event_handle函数
    esp_event_handler_register(WIFI_EVENT,ESP_EVENT_ANY_ID,wifi_event_handle,NULL);
    // 注册IP获取事件（IP_EVENT_STA_GOT_IP）到wifi_event_handle函数
    esp_event_handler_register(IP_EVENT,IP_EVENT_STA_GOT_IP,wifi_event_handle,NULL);

    // 7. 配置要连接的Wi-Fi参数（热点名称、密码、加密方式等）
    wifi_config_t wifi_config = {
        .sta.threshold.authmode = WIFI_AUTH_WPA2_PSK,// 加密方式（WPA2-PSK，常见家用Wi-Fi加密）
        .sta.pmf_cfg.capable = true,// 支持PMF
        .sta.pmf_cfg.required = false,// 不强制要求路由器支持PMF
    };
    // 清空SSID数组（避免残留垃圾数据）
    memset(&wifi_config.sta.ssid,0,sizeof(wifi_config.sta.ssid));
    // 将TEST_SSID（热点名称）复制到Wi-Fi配置中
    memcpy(wifi_config.sta.ssid,TEST_SSID,strlen(TEST_SSID));

    // 清空密码数组（避免残留垃圾数据）
    memset(&wifi_config.sta.password,0,sizeof(wifi_config.sta.password));
    // 将TEST_PASSWORD（密码）复制到Wi-Fi配置中
    memcpy(wifi_config.sta.password,TEST_PASSWORD,strlen(TEST_PASSWORD));


    // 8. 设置Wi-Fi工作模式为STA（客户端模式，连接其他热点）
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    // 应用Wi-Fi配置（将上面设置的热点信息传入Wi-Fi驱动）
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA,&wifi_config));
    // 启动Wi-Fi（开始执行连接流程）
    ESP_ERROR_CHECK(esp_wifi_start());

    xSemaphoreTake(s_wifi_connect_sem,portMAX_DELAY);
    mqtt_start();
    int count = 0;
    while(1)
    {
        char count_str[32];
        snprintf(count_str,sizeof(count_str),"{\"count\":%d}",count);
        esp_mqtt_client_publish(mqtt_handle,MQTT_TOPIC1,count_str,strlen(count_str),1,0);
        count++;
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
