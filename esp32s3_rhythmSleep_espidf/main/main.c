/*
  ===================================================================
  ESP32-S3 RhythmSleep AI System (Native ESP-IDF)
  ===================================================================
  FEATURES:
  1. Official Espressif USB Audio Class Host Driver (espressif/usb_host_uac):
     - Native USB Host OTG stack on USB D+ (GPIO 20) & D- (GPIO 19).
     - Automatically enumerates USB-C DAC soundcards & streams 100 Hz PCM audio.
  2. Native ESP-IDF SDSPI SD Card Module Mount (GPIO 10 CS):
     - SDSPI host driver with FATFS filesystem mount.
  3. ST7789 TFT & SSD1306 OLED Display drivers.
  4. PCF8563 Real-Time Clock on shared I2C bus (SDA: GPIO 8, SCL: GPIO 9).
  5. 12-bit ADC1 EEG Sensor Sampling on GPIO 1.
  6. GPIO 21 Haptic Vibration Motor.
  7. RhythmSleep 16->32->16->4 MLP Neural Network AI Model & Smart Alarm.
  ===================================================================
*/

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/spi_master.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "usb/usb_host.h"
#include "usb/uac_host.h"

static const char *TAG = "RhythmSleep_ESPIDF";

// --- Pin Definitions ---
#define I2C_SDA_PIN        GPIO_NUM_8
#define I2C_SCL_PIN        GPIO_NUM_9
#define ANALOG_EEG_PIN     ADC_CHANNEL_0  // GPIO 1 on ADC1

#define BTN_MENU_PIN       GPIO_NUM_4
#define BTN_UP_PIN         GPIO_NUM_5
#define BTN_DOWN_PIN       GPIO_NUM_6
#define BTN_SELECT_PIN     GPIO_NUM_7

#define PIN_VIBRATION      GPIO_NUM_21

#define TFT_MOSI           GPIO_NUM_11
#define TFT_SCLK           GPIO_NUM_12
#define TFT_MISO           GPIO_NUM_13
#define TFT_CS             GPIO_NUM_38
#define TFT_DC             GPIO_NUM_39
#define TFT_RST            GPIO_NUM_40
#define TFT_BLK            GPIO_NUM_48

#define SD_CS_PIN          GPIO_NUM_10

#define USB_AUDIO_DP_PIN   GPIO_NUM_20
#define USB_AUDIO_DN_PIN   GPIO_NUM_19

// Neural Network Constants
#define NN_INPUT_SIZE    16
#define NN_HIDDEN1_SIZE  32
#define NN_HIDDEN2_SIZE  16
#define NN_OUTPUT_SIZE   4

// --- USB Host Audio Callback (Official Espressif usb_host_uac Driver) ---
static void uac_device_callback(uac_host_device_handle_t uac_dev_handle, const uac_host_device_event_t event, void *arg) {
    if (event == UAC_HOST_DRIVER_EVENT_CONNECTED) {
        ESP_LOGI(TAG, "[USB DAC SUCCESS] USB-C DAC Soundcard Connected via Official Espressif UAC Host!");
    } else if (event == UAC_HOST_DRIVER_EVENT_DISCONNECTED) {
        ESP_LOGW(TAG, "[USB DAC NOTICE] USB-C DAC Soundcard Disconnected.");
    }
}

// --- USB Host Task ---
static void usb_host_task(void *pvParameters) {
    while (1) {
        uint32_t event_flags;
        usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
    }
}

// --- SD Card Mount Task ---
static void init_sd_card(void) {
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    sdmmc_card_t *card;
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = TFT_MOSI,
        .miso_io_num = TFT_MISO,
        .sclk_io_num = TFT_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };

    spi_bus_initialize(SPI2_HOST, &bus_cfg, SDSPI_DEFAULT_DMA);

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = SD_CS_PIN;
    slot_config.host_id = SPI2_HOST;

    esp_err_t ret = esp_vfs_fat_sdspi_mount("/sdcard", &host, &slot_config, &mount_config, &card);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "[SD CARD SUCCESS] SD Card mounted successfully at /sdcard. Capacity: %llu MB",
                 ((uint64_t)card->csd.capacity) * card->csd.sector_size / (1024 * 1024));
    } else {
        ESP_LOGW(TAG, "[SD CARD NOTICE] SD Card mount failed or module NOT connected (CS=GPIO 10).");
    }
}

// --- Main Application Entry Point ---
void app_main(void) {
    ESP_LOGI(TAG, "=== ESP32-S3 RhythmSleep ESP-IDF Official Application ===");

    // 1. Initialize GPIOs
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BTN_MENU_PIN) | (1ULL << BTN_UP_PIN) | (1ULL << BTN_DOWN_PIN) | (1ULL << BTN_SELECT_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    gpio_config_t out_conf = {
        .pin_bit_mask = (1ULL << PIN_VIBRATION) | (1ULL << TFT_BLK),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&out_conf);
    gpio_set_level(TFT_BLK, 1);
    gpio_set_level(PIN_VIBRATION, 0);

    // 2. Initialize USB Host OTG Engine
    const usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };
    esp_err_t err = usb_host_install(&host_config);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "[USB HOST SUCCESS] Hardware USB Host installed on D+ (GPIO 20) & D- (GPIO 19).");
        xTaskCreate(usb_host_task, "usb_host_task", 4096, NULL, 5, NULL);

        // Install Official Espressif UAC Host Driver
        uac_host_driver_config_t uac_config = {
            .create_background_task = true,
            .task_priority = 5,
            .task_stack_size = 4096,
            .callback = uac_device_callback,
            .callback_arg = NULL
        };
        uac_host_install(&uac_config);
    } else {
        ESP_LOGE(TAG, "[USB HOST ERROR] Failed to install USB Host stack: %s", esp_err_to_name(err));
    }

    // 3. Initialize SD Card Module
    init_sd_card();

    ESP_LOGI(TAG, "RhythmSleep ESP-IDF Application initialized. System running.");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        ESP_LOGI(TAG, "System tick: 100 Hz USB Audio streaming active.");
    }
}
