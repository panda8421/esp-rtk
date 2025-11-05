#include <stdio.h>
#include "driver/uart.h"   // UART 驱动库：提供 UART 初始化、读写等函数
#include "esp_log.h"       // ESP 日志库：用于打印调试信息
#include "driver/gpio.h"   // GPIO 驱动库：虽然未直接使用，但可能为后续扩展预留

// 定义使用的 UART 端口为 UART0（ESP32-C3 中通常对应默认串口，如 USB 转串口）
#define USER_UART    UART_NUM_0

// 日志标签：用于区分不同模块的日志输出（此处为 "uart0"）
#define TAG     "uart0"

// 定义 UART 接收缓冲区：用于临时存储接收到的数据，大小 1024 字节
static uint8_t uart_buffer[1024];

// 定义 UART 事件队列句柄：用于接收 UART 驱动发送的事件（如数据到达、缓冲区满等）
static QueueHandle_t  uart_queue;

void app_main(void)  // 应用程序入口函数（ESP-IDF 中程序从这里开始执行）
{
    uart_event_t uart_ev;  // 声明 UART 事件变量：用于存储从队列接收到的事件

    // 配置 UART 参数结构体
    uart_config_t uart_cfg = 
    {
        .baud_rate = 115200,          // 波特率：115200（常用串口通信速率）
        .data_bits = UART_DATA_8_BITS, // 数据位：8 位
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE, // 硬件流控：禁用（无需 RTS/CTS 引脚）
        .parity = UART_PARITY_DISABLE, // 校验位：禁用（原代码此处笔误，应为 UART_PARITY_DISABLE）
        .source_clk = UART_SCLK_DEFAULT, // 时钟源：默认时钟（由 ESP-IDF 自动管理）
        .stop_bits = UART_STOP_BITS_1,  // 停止位：1 位
    };

    // 应用 UART 参数配置（将上面定义的 uart_cfg 应用到 USER_UART 端口）
    uart_param_config(USER_UART, &uart_cfg);

    //uart_set_pin(USER_UART,31,30,-1,-1);    //这个是设置串口的端口号，当你不用默认的串口引脚需要自己设定特定IO时，才用到


    // 安装 UART 驱动并创建事件队列
    // 参数说明：
    // USER_UART：目标 UART 端口
    // 1024：接收缓冲区大小（字节）
    // 1024：发送缓冲区大小（字节）
    // 20：队列可存储的最大事件数
    // &uart_queue：事件队列句柄（传出参数，驱动会将事件发送到该队列）
    // 0：中断优先级（默认即可）
    uart_driver_install(USER_UART, 1024, 1024, 20, &uart_queue, 0);

    // 主循环：持续处理 UART 事件
    while(1)
    {
        // 从 UART 事件队列中接收事件（阻塞等待，直到有事件到来）
        // 参数说明：
        // uart_queue：目标队列句柄
        // &uart_ev：接收事件的变量（传出参数）
        // portMAX_DELAY：无限等待（直到有事件，不超时）
        if(pdTRUE == xQueueReceive(uart_queue, &uart_ev, portMAX_DELAY))
        {
            // 根据事件类型处理不同情况
            switch(uart_ev.type)
            {
                case UART_DATA:  // 事件类型：接收到数据
                    // 打印接收数据的长度（日志级别：INFO）
                    ESP_LOGI(TAG, "UART0 receive len:%i", uart_ev.size);
                    // 读取 UART 接收缓冲区的数据到 uart_buffer
                    // 参数：UART 端口、目标缓冲区、读取长度、超时时间（1000ms）
                    uart_read_bytes(USER_UART, uart_buffer, uart_ev.size, pdMS_TO_TICKS(1000));
                    // 将接收到的数据原样回发（实现回声功能）
                    uart_write_bytes(USER_UART, uart_buffer, uart_ev.size);
                    break;

                case UART_BUFFER_FULL:  // 事件类型：接收缓冲区满
                    uart_flush_input(USER_UART);  // 清空接收缓冲区
                    xQueueReset(uart_queue);       // 重置事件队列（清除未处理事件）
                    break;

                case UART_FIFO_OVF:  // 事件类型：硬件 FIFO 溢出（数据丢失）
                    uart_flush_input(USER_UART);  // 清空接收缓冲区
                    xQueueReset(uart_queue);       // 重置事件队列
                    break;

                default:  // 其他未处理事件
                    break;
            }
        }
    }
}