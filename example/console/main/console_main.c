/* Basic console example (esp_console_repl API)

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>// 标准输入输出（如 printf）
#include <string.h> // 字符串处理（如 strcmp）
#include "esp_system.h"// ESP 系统基础功能（如复位、版本）
#include "esp_log.h" // ESP 日志模块（打印调试信息）
#include "esp_console.h"// ESP 控制台核心 API（命令注册、REPL 交互）
#include "esp_vfs_dev.h" // ESP VFS（虚拟文件系统）设备适配（如串口、USB）
#include "esp_vfs_fat.h"// ESP VFS-FAT 文件系统适配（用于存储命令历史）
#include "nvs.h"// NVS（非易失性存储）基础接口
#include "nvs_flash.h"// NVS Flash 初始化与操作
#include "cmd_system.h"// 系统相关命令（如重启、查看版本，官方预定义命令）
#include "cmd_wifi.h"// WiFi 相关命令（如连接热点、查看状态，官方预定义命令）
#include "cmd_nvs.h"// NVS 相关命令（如读写 NVS 数据，官方预定义命令）

/* 
 * 警告：若启用了「次级串口控制台」，会打印此提示。
 * 原因：次级串口控制台仅支持「输出」，不支持「输入」，而交互式控制台需要输入（如敲命令），因此次级控制台无实际用途。
 * 解决：若看到此警告，在 menuconfig 中禁用次级串口控制台（路径：Component config → ESP System Settings → Secondary console）。
 */
#if SOC_USB_SERIAL_JTAG_SUPPORTED// 判断芯片是否支持 USB Serial/JTAG（如 ESP32-C3、S3） 
#if !CONFIG_ESP_CONSOLE_SECONDARY_NONE// 判断是否未禁用次级控制台
#warning "A secondary serial console is not useful when using the console component. Please disable it in menuconfig."
#endif
#endif

static const char* TAG = "example";// 日志标签（打印日志时用于区分模块）
#define PROMPT_STR CONFIG_IDF_TARGET// 控制台提示符前缀，值为芯片型号（如 "esp32c3"，由 CONFIG_IDF_TARGET 自动定义）

/* 
 * 控制台命令历史功能：可将历史命令存储到文件、从文件加载（避免重启后丢失历史）。
 * 实现方式：基于 Wear Levelling（磨损均衡）库，在 Flash 上挂载 FATFS 文件系统，用于存储历史文件。
 * 注：此功能需在 menuconfig 中启用（CONFIG_CONSOLE_STORE_HISTORY）。
 */
#if CONFIG_CONSOLE_STORE_HISTORY // 若启用了「命令历史存储」配置

#define MOUNT_PATH "/data" // FATFS 文件系统挂载路径（后续访问文件需用此路径前缀）
#define HISTORY_PATH MOUNT_PATH "/history.txt"// 命令历史文件路径（历史命令会写入此文件）

/**
 * @brief 初始化文件系统（用于存储命令历史）
 * 功能：在 Flash 的 "storage" 分区上挂载 FATFS，开启读写权限，支持磨损均衡。
 */
static void initialize_filesystem(void)
{
    static wl_handle_t wl_handle;// 磨损均衡句柄（静态变量，确保生命周期与程序一致）
    // FATFS 挂载配置：最大支持 4 个同时打开的文件，若挂载失败则自动格式化分区
    const esp_vfs_fat_mount_config_t mount_config = {
            .max_files = 4,
            .format_if_mount_failed = true
    };
    // 执行挂载：参数依次为「挂载路径」「Flash 分区名」「挂载配置」「磨损均衡句柄」
    esp_err_t err = esp_vfs_fat_spiflash_mount_rw_wl(MOUNT_PATH, "storage", &mount_config, &wl_handle);
    if (err != ESP_OK) {// 若挂载失败，打印错误日志（不终止程序，仅禁用历史功能）
        ESP_LOGE(TAG, "Failed to mount FATFS (%s)", esp_err_to_name(err));
        return;
    }
}
#endif // CONFIG_STORE_HISTORY


/**
 * @brief 初始化 NVS（非易失性存储）
 * 功能：NVS 用于存储设备配置（如 WiFi 密码、用户参数），控制台部分命令（如 cmd_nvs）依赖 NVS 工作。
 * 处理逻辑：若 NVS 无空闲页或版本不匹配，先擦除 NVS 再初始化。
 */
static void initialize_nvs(void)
{
    esp_err_t err = nvs_flash_init();// 初始化 NVS Flash
     // 处理两种常见错误：1. NVS 无空闲页；2. NVS 版本更新（需擦除旧数据）
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK( nvs_flash_erase() );// 擦除 NVS 所有数据
        err = nvs_flash_init();// 重新初始化 NVS
    }
    ESP_ERROR_CHECK(err);// 若仍有错误，终止程序（NVS 初始化失败会影响后续功能）
}
//打印HELLOWORLD
static int helloworld_cmd(int argc, char **argv)
{
    ESP_LOGI(TAG,"you enter the helloworld cmd");
    
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
    const esp_console_cmd_t cmd1 = {
        .command = "helloworld",
        .help = "this command just for test helloworld",
        .hint = NULL,
        .func = helloworld_cmd,
    };
    esp_console_cmd_register(&cmd1);

    const esp_console_cmd_t cmd_led = {
    .command = "led",                // 命令名（用户输入的关键词）
    .help = "控制 LED 开关，用法：led on/off", // 帮助信息（输入 "help led" 时显示）
    .hint = "<on/off>",              // 命令提示（输入 "led " 按 Tab 键会显示）
    .func = cmd_led_control          // 绑定命令的处理函数
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd_led)); // 注册到控制台

}


void app_main(void)
{
    esp_console_repl_t *repl = NULL;// REPL 句柄（指向控制台交互实例，后续用于控制控制台生命周期）
    //初始化 REPL 配置：使用默认配置（可后续修改提示符、命令长度等）
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    /* 自定义控制台提示符：格式为「芯片型号>」（如 "esp32c3>"）
     * 作用：提示用户可输入命令，可根据需求修改（如改为 "cmd>"）
     */
    repl_config.prompt = PROMPT_STR ">";
    // 最大命令行长度：由配置项 CONFIG_CONSOLE_MAX_COMMAND_LINE_LENGTH 定义（默认 256 字符）
    repl_config.max_cmdline_length = CONFIG_CONSOLE_MAX_COMMAND_LINE_LENGTH;
    // 3. 初始化基础依赖模块
    initialize_nvs();// 先初始化 NVS（控制台命令可能依赖 NVS）

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
