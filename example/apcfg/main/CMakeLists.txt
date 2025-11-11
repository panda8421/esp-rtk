idf_component_register(SRCS "wifi_manager.c" "apcfg.c" "ap_wifi.c" "ws_server.c"
                    INCLUDE_DIRS ".")

spiffs_create_partition_image(html ../html FLASH_IN_PROJECT)