#include "onenet_dm.h"
#include "esp_log.h"
#include <string.h>

static int led_brightness = 0;   //LED亮度
static int led_status = 0;      //LED开关状态

static int ws2812_red = 0;      //RGB红色通道
static int ws2812_green = 0;    //RGB绿色通道
static int ws2812_blue = 0;     //RGB蓝色通道

void onenet_dm_init(void)
{

}
void onenet_property_handle(cJSON* property)
{
    // {
    // "id": "123",
    // "version": "1.0",
    // "params": {
    //     "Brightness":"50"
    //     "LightSwitch":true
    //     "RGBColor":{
    //          "Red":100,
    //          "Green":100,
    //          "Blue":100,
    //      }
    // }
    // }

     // 先判断传入的property是否为空，避免空指针访问
    if (property == NULL) {
        ESP_LOGE("onenet", "property is NULL, skip handle");
        return;
    }
    //从CJSON中解析”param“字段
    cJSON* param_js = cJSON_GetObjectItem(property,"params");
    if (param_js == NULL) {
        ESP_LOGE("onenet", "params not found in property");
        return;
    }
    if(param_js)
    {
        cJSON* name_js = param_js->child;
        while(name_js)
        {
            if(strcmp(name_js->string,"Brightness") == 0)
            {
                led_brightness = cJSON_GetNumberValue(name_js);
                ESP_LOGI("onenet", "Set Brightness to %d", led_brightness);
            }
            else if(strcmp(name_js->string,"LightSwitch") == 0)  //开关数据
            {
                if(cJSON_IsTrue(name_js))
                {
                    led_status = 1;
                    ESP_LOGI("onenet", "Turn LED ON");
                }
                else
                {
                    led_status = 0;
                    ESP_LOGI("onenet", "Turn LED OFF");
                }
            }
            else if(strcmp(name_js->string,"RGBColor") == 0)
            {
                ws2812_red = cJSON_GetNumberValue(cJSON_GetObjectItem(name_js,"Red"));
                ws2812_green = cJSON_GetNumberValue(cJSON_GetObjectItem(name_js,"Green"));
                ws2812_blue = cJSON_GetNumberValue(cJSON_GetObjectItem(name_js,"Blue"));

                ESP_LOGI("onenet", "Set RGB to (%d, %d, %d)", ws2812_red, ws2812_green, ws2812_blue);
            }
        name_js = name_js->next;
        }
    }
}

cJSON* onenet_property_upload(void)
{
    // {
    // "id": "123",
    // "version": "1.0",
    // "params": {
    //     "Brightness":
    //      {
    //           "value":50 
    //       }
    //     "LightSwitch":
    //       {
    //            "value":true
    //        }
    //     "RGBColor":{
    //          "value":{               
    //          "Red":100,
    //          "Green":100,
    //          "Blue":100,
    //          }
    //      }
    // }
    // }
    //创建根JSON对象
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root,"id","123");
    cJSON_AddStringToObject(root,"version","1.0");
    cJSON* param_js = cJSON_AddObjectToObject(root,"params");
    //亮度
    cJSON* led_brightness_js = cJSON_AddObjectToObject(param_js,"Brightness");
    cJSON_AddNumberToObject(led_brightness_js,"value",led_brightness);

    //开关
    cJSON* lightness_js = cJSON_AddObjectToObject(param_js,"LightSwitch");
    cJSON_AddBoolToObject(lightness_js,"value",led_status);

    //RGB值
    cJSON* color_js = cJSON_AddObjectToObject(param_js,"RGBColor");
    cJSON* color_value_js = cJSON_AddObjectToObject(color_js,"value");
    cJSON_AddNumberToObject(color_value_js,"Red",ws2812_red);
    cJSON_AddNumberToObject(color_value_js,"Green",ws2812_green);
    cJSON_AddNumberToObject(color_value_js,"Blue",ws2812_blue);
    return root;
}