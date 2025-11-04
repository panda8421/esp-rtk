#include "esp_ota_ops.h"   //引入 ESP32 OTA相关操作库

const char* get_app_versionvoid()
{
    // 静态缓冲区，用于存储版本号（仅初始化一次，避免重复分配内存）
    static char app_version[32] = {0};
    // 如果缓冲区未初始化（首次调用），则读取版本信息
    if(app_version[0] == 0)
    {
        // 获取当前正在运行的固件分区（OTA 中可能有多个固件分区，这里定位当前激活的分区）
        const esp_partition_t * running = esp_ota_get_running_partition();
          // 存储固件描述信息的结构体（包含版本号、编译时间等）
        esp_app_desc_t app_desc;
        // 从当前运行的分区中读取固件描述信息（包括 version 字段）
        esp_ota_get_partition_description(running,&app_desc);
          // 将固件版本号（app_desc.version）复制到缓冲区中
        snprintf(app_version,sizeof(app_version),"%s",app_desc.version);
    }
    return app_version;
}

void set_app_valid(int valid)
{
     // 获取当前正在运行的固件分区
    const esp_partition_t * running = esp_ota_get_running_partition();
    esp_ota_img_states_t state; // 存储当前固件的状态（如：待验证、有效、无效等）
    // 获取当前固件分区的状态（检查是否处于"待验证"状态）
    if(esp_ota_get_state_partition(running,&state) == ESP_OK)
    {
        // 仅当固件处于"待验证"状态时，才执行标记操作
        // （OTA 升级后，新固件首次启动时会处于此状态，等待确认）
        if(state == ESP_OTA_IMG_PENDING_VERIFY)
        {
            if(valid)
            {
                 // 标记当前固件为有效，取消回滚（确认升级成功）
                esp_ota_mark_app_valid_cancel_rollback();
            }
            else
            {
                // 标记当前固件为无效，触发回滚到上一版本并重启（升级失败）
                esp_ota_mark_app_invalid_rollback_and_reboot();
            }
        }

    }
}