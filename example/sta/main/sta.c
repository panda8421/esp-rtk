#include <stdio.h>
#include "nvs_flash.h"// NVS闪存初始化库（用于存储Wi-Fi等配置信息的非易失性存储）
#include "esp_wifi.h"// Wi-Fi功能库（ESP32 Wi-Fi相关API）
#include "esp_event.h"// 事件处理库（用于处理Wi-Fi连接、IP获取等事件）
#include "esp_log.h"// 日志库（用于打印调试信息）
#include "esp_err.h"// 错误处理库（定义ESP32错误码）
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "lwip/sockets.h"  //TCP通信需要的头文件

#define TEST_SSID "Xiaomi 14 Pro"// 要连接的Wi-Fi热点名称（SSID）
#define TEST_PASSWORD "12345678"// 要连接的Wi-Fi密码

#define TAG "sta"// 日志标签（打印日志时的标识，方便筛选）

// 手机TCP服务器信息（手机IP和端口）
#define PHONE_IP "192.168.1.99"  // 手机IP（网关地址）
#define PHONE_PORT 8080             // 手机端口（和调试助手一致）

// 全局变量：标记是否已获取IP（用于判断是否可以发送数据）
static bool has_ip = false;

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
                ESP_LOGE(TAG,"esp32 connect the ap faild! retry!");
                vTaskDelay(pdMS_TO_TICKS(1000));
                esp_wifi_connect(); // 自动重试连接
                has_ip = false;  // 断开连接后重置IP标记
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
                has_ip = true; //获取IP后标记为可发送数据
                break;
        }
    }
}

// 发送数据的任务：周期性向手机发送数据
static void send_data_task(void *arg)
{
    int sockfd;  //  socket描述符
    struct sockaddr_in server_addr;
    char recv_buf[100];  // 接收缓冲区
    while (1) 
    {
        //等待获取IP后再连接服务器
        if (!has_ip) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        // 1. 创建TCP socket
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            ESP_LOGE(TAG, "Failed to create socket");
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }

        // 2. 配置服务器地址（手机IP和端口）
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;   //使用 IPv4 地址格式,例如：192.168.1.100，这样的格式
        server_addr.sin_port = htons(PHONE_PORT);  // 端口转换为网络字节序
        // 将手机IP字符串转换为网络地址
        if (inet_pton(AF_INET, PHONE_IP, &server_addr.sin_addr) <= 0) {
            ESP_LOGE(TAG, "Invalid phone IP address");
            close(sockfd);
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }

        // 3. 连接手机的TCP服务器
        if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0) {
            ESP_LOGE(TAG, "Failed to connect to phone server");
            close(sockfd);
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }
        ESP_LOGI(TAG, "Connected to phone server! 可发送指令控制（LED_ON/LED_OFF）");

        // 循环：同时发送温度数据和接收手机指令
        while (has_ip) {
            // 生成随机温度（10~30℃）
            int temp = rand() % 21 + 10;  // 随机数：10-30
            char data[50];
            sprintf(data, "ESP32温度: %d℃\n", temp);  // 格式化数据

            // 发送数据到手机
            int send_len = send(sockfd, data, strlen(data), 0);
            if (send_len < 0) {
                ESP_LOGE(TAG, "Failed to send data");
                break;  // 发送失败则断开重连
            }
            ESP_LOGI(TAG, "发送数据: %s", data);

            
            // 2. 接收手机指令（非阻塞，超时1秒）
            struct timeval timeout = {1, 0};  // 1秒超时
            setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
            int recv_len = recv(sockfd, recv_buf, sizeof(recv_buf)-1, 0);
            if (recv_len > 0) {
                recv_buf[recv_len] = '\0';  // 加结束符
                ESP_LOGI(TAG, "收到手机指令: %s", recv_buf);
                // 模拟控制LED
                if (strcmp(recv_buf, "LED_ON") == 0) {
                    ESP_LOGI(TAG, "指令执行: LED点亮");
                } else if (strcmp(recv_buf, "LED_OFF") == 0) {
                    ESP_LOGI(TAG, "指令执行: LED熄灭");
                } else {
                    ESP_LOGI(TAG, "指令未知: %s", recv_buf);
                }
            }
            vTaskDelay(pdMS_TO_TICKS(2000));  // 每2秒发一次
        }

                // 5. 断开连接
        close(sockfd);
        ESP_LOGI(TAG, "Disconnected from server");
        vTaskDelay(pdMS_TO_TICKS(1000));

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

    // 创建发送数据的任务（独立于main_task，后台运行）
    xTaskCreate(send_data_task, "send_data_task", 4096, NULL, 5, NULL);

}
