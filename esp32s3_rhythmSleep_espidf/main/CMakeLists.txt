idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES driver esp_adc esp_lcd fatfs sdmmc usb_host_uac
)
