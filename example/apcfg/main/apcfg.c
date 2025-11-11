#include <stdio.h>
#include <string.h> // 字符串处理（如 strcmp）
#include "ap_wifi.h"
#include "nvs_flash.h" 
#include "wifi_manager.h"
#include "esp_log.h" 
#include "esp_console.h"// ESP 控制台核心 API（命令注册、REPL 交互）
#include "cmd_system.h"// 系统相关命令（如重启、查看版本，官方预定义命令）
#include "cmd_wifi.h"// WiFi 相关命令（如连接热点、查看状态，官方预定义命令）
#include "cmd_nvs.h"// NVS 相关命令（如读写 NVS 数据，官方预定义命令）

// 日志标签，用于区分不同模块的日志输出
#define TAG     "main"

#define PROMPT_STR CONFIG_IDF_TARGET// 控制台提示符前缀，值为芯片型号（如 "esp32c3"，由 CONFIG_IDF_TARGET 自动定义）

/**
 * @brief WiFi状态变化的处理函数（回调函数，由WiFi模块触发）
 * @param state 当前WiFi状态（枚举类型，如连接/断开）
 */
void wifi_state_handle(WIFI_STATE state)
{
    if (state == WIFI_STATE_CONNECTED) {
        // 若WiFi连接成功，输出日志
        ESP_LOGI(TAG, "Wifi connected");
    }
    else if (state == WIFI_STATE_DISCONNECTED) {  // 原代码此处可能笔误，应为DISCONNECTED
        // 若WiFi断开连接，输出日志
        ESP_LOGI(TAG, "Wifi disconnected");
    }
}
//启动AP配网
static int ap_wifi_start(int argc, char **argv)
{
    ap_wifi_apcfg();
    ESP_LOGI(TAG,"Start AP WIFI NET");
    return 0;
}
//LED 控制命令的处理函数（输入 "led on" 或 "led off" 时执行）
static int cmd_led_control(int argc, char **argv)
{
    // 检查参数数量：必须输入 "led on" 或 "led off"（argc=2）
    if (argc != 2) {
        ESP_LOGI(TAG,"用法错误！正确用法：led on/off\n");
        return ESP_ERR_INVALID_ARG; // 返回错误码，控制台会提示
    }
    // 处理 "on" / "off" 命令
    if (strcmp(argv[1], "on") == 0) {
        ESP_LOGI(TAG,"LED 已点亮（GPIO15）\n");
    } else if (strcmp(argv[1], "off") == 0) {
        ESP_LOGI(TAG,"LED 已熄灭（GPIO15）\n");
    } else {
        ESP_LOGI(TAG,"参数错误！仅支持 on/off\n");
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK; // 返回成功，控制台无额外提示
}


//注册命令
void set_cmd_init(void)
{
    //打印helloworld命令
    const esp_console_cmd_t cmd_ap_wifi = {
        .command = "ap_wifi_net_start",
        .help = "this command just for Start AP distribution network",
        .hint = NULL,
        .func = ap_wifi_start,
    };
    esp_console_cmd_register(&cmd_ap_wifi);

    const esp_console_cmd_t cmd_led = {
    .command = "led",                // 命令名（用户输入的关键词）
    .help = "控制 LED 开关，用法：led on/off", // 帮助信息（输入 "help led" 时显示）
    .hint = "<on/off>",              // 命令提示（输入 "led " 按 Tab 键会显示）
    .func = cmd_led_control          // 绑定命令的处理函数
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd_led)); // 注册到控制台

}
void init_console(void)
{
    esp_console_repl_t *repl = NULL;// REPL 句柄（指向控制台交互实例，后续用于控制控制台生命周期）
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();        //初始化 REPL 配置：使用默认配置（可后续修改提示符、命令长度等）
    repl_config.prompt = PROMPT_STR ">";    //自定义控制台提示符
    repl_config.max_cmdline_length = CONFIG_CONSOLE_MAX_COMMAND_LINE_LENGTH;    // 最大命令行长度：由配置项 CONFIG_CONSOLE_MAX_COMMAND_LINE_LENGTH 定义（默认 256 字符）

    // 若启用命令历史存储，初始化文件系统并配置历史文件路径
#if CONFIG_CONSOLE_STORE_HISTORY
    initialize_filesystem();   // 挂载 FATFS
    repl_config.history_save_path = HISTORY_PATH; // 指定历史命令存储文件
    ESP_LOGI(TAG, "Command history enabled");// 打印日志：历史功能已启用
#else
    ESP_LOGI(TAG, "Command history disabled");
#endif
 // 4. 注册控制台命令（用户输入的命令需先注册才能被识别）
    esp_console_register_help_command();// 注册 "help" 命令（查看所有支持的命令）
    register_system_common(); // 注册系统通用命令（如 "restart" 重启、"version" 查看版本）
    //register_system_sleep();
#if CONFIG_ESP_WIFI_ENABLED // 若启用了 WiFi 配置，注册 WiFi 相关命令
    register_wifi();// 注册 WiFi 命令（如 "wifi connect" 连接热点、"wifi scan" 扫描热点
#endif
    register_nvs();// 注册 NVS 命令（如 "nvs get" 读取 NVS、"nvs set" 写入 NVS）
    //注册自己的命令
    set_cmd_init();
        // 5. 根据配置选择控制台硬件接口（UART / USB CDC / USB Serial/JTAG）
    // 逻辑：通过 menuconfig 配置的「控制台类型」，初始化对应硬件并创建 REPL 实例
#if defined(CONFIG_ESP_CONSOLE_UART_DEFAULT) || defined(CONFIG_ESP_CONSOLE_UART_CUSTOM)
 // 情况1：使用 UART 作为控制台（默认 UART 或自定义 UART）
    esp_console_dev_uart_config_t hw_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT(); // UART 默认配置（波特率 115200 等）
    // 创建 UART 类型的 REPL 实例：参数为「UART 配置」「REPL 配置」「REPL 句柄指针」
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&hw_config, &repl_config, &repl));

#elif defined(CONFIG_ESP_CONSOLE_USB_CDC)
 // 情况2：使用 USB CDC 作为控制台（如 ESP32-S3 的 USB 虚拟串口）
    esp_console_dev_usb_cdc_config_t hw_config = ESP_CONSOLE_DEV_CDC_CONFIG_DEFAULT();// USB CDC 默认配置
    ESP_ERROR_CHECK(esp_console_new_repl_usb_cdc(&hw_config, &repl_config, &repl));

#elif defined(CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG)
    // 情况3：使用 USB Serial/JTAG 作为控制台（如 ESP32-C3 的原生 USB 接口）
    esp_console_dev_usb_serial_jtag_config_t hw_config = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_serial_jtag(&hw_config, &repl_config, &repl));

#else
#error Unsupported console type
#endif
    // 6. 启动交互式控制台（REPL：Read-Eval-Print Loop，读-评-印循环）
    // 功能：启动后控制台会持续等待用户输入→解析命令→执行命令→打印结果，直到程序退出
    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}
/**
 * 板级初始化，自己在这个函数加初始化代码
 * @param 无
 * @return 无
 */
void board_init(void)
{
    esp_err_t ret_val = ESP_OK;
    ret_val = nvs_flash_init();
    if (ret_val == ESP_ERR_NVS_NO_FREE_PAGES || ret_val == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret_val = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret_val );
}

void app_main(void)
{
    //板级初始化
    board_init();
    init_console();//控制台初始化
    ap_wifi_init(wifi_state_handle);
}
