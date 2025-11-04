#ifndef _ONENET_MQTT_H_
#define _ONENET_MQTT_H_
#include "esp_err.h"

//产品ID
#define ONENET_PRODUCT_ID            "E3Sgq5fGjC"  
//产品密钥
#define ONENET_PRODUCT_ACCESS_KEY    "a82bv2/LWRePdSfT+cmRDw2Jt+8bl3NZJxOxbdN3O6M="
//设备名称                  
#define ONENET_DEVICE_NAME           "esp32led01"

esp_err_t onenet_start(void);

esp_err_t onenet_post_property_data(const char* data);

#endif