#ifndef _ONENET_DM_H
#define _ONENET_DM_H
#include "cJSON.h"
 
void onenet_dm_init(void);      //初始化物模型数据
void onenet_property_handle(cJSON* property);    //处理onenet给ESP32下发的数据

cJSON* onenet_property_upload(void);   //处理ESP32给onenet上报的数据

#endif