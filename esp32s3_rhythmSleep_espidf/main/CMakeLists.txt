idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES driver fatfs sdmmc esp_lcd usb usb_host_uac esp_timer esp_adc)
