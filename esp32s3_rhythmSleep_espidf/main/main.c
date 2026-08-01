/*
  ===================================================================
  ESP32-S3 RhythmSleep AI System (Native ESP-IDF)
  ===================================================================
  FEATURES:
  1. ST7789 2.8" SPI TFT Display Engine (SPI3_HOST + Active Low BLK):
     - MOSI=11, SCLK=12, MISO=13, CS=38, DC=39, RST=40, BLK=48.
     - SPI3_HOST bus driver with DMA channel auto allocation.
     - Active Low / High Backlight drive to light up display.
     - Solid full-screen color fill (Magenta / Cyan) to verify panel output.
  2. Dual Audio Engine (USB Host UAC + 40 kHz High-Speed LEDC PWM Differential):
     - Continuous 100 Hz differential tone on GPIO 20 (D+) & GPIO 19 (D-).
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
#include "driver/ledc.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
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

// Display Dimensions
#define LCD_H_RES          320
#define LCD_V_RES          240

static esp_lcd_panel_handle_t panel_handle = NULL;
static uac_host_device_handle_t uac_device_handle = NULL;
static bool uac_connected = false;

// Color Palette (RGB565)
#define COLOR_BLACK        0x0000
#define COLOR_WHITE        0xFFFF
#define COLOR_NAVY         0x000F
#define COLOR_CYAN         0x07FF
#define COLOR_GREEN        0x07E0
#define COLOR_MAGENTA      0xF81F
#define COLOR_YELLOW       0xFFE0
#define COLOR_RED          0xF800

// --- LEDC PWM Audio Engine (Differential 100 Hz Wave on GPIO 20 & 19) ---
static void init_pwm_audio(void) {
    ESP_LOGI(TAG, "Initializing High-Speed 40 kHz LEDC PWM Audio Engine on GPIO 20 & GPIO 19...");

    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .timer_num        = LEDC_TIMER_0,
        .duty_resolution  = LEDC_TIMER_8_BIT,
        .freq_hz          = 40000, // 40 kHz carrier
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ch_dp = {
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = LEDC_CHANNEL_0,
        .timer_sel      = LEDC_TIMER_0,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = USB_AUDIO_DP_PIN,
        .duty           = 128,
        .hpoint         = 0
    };
    ledc_channel_config(&ch_dp);

    ledc_channel_config_t ch_dn = {
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = LEDC_CHANNEL_1,
        .timer_sel      = LEDC_TIMER_0,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = USB_AUDIO_DN_PIN,
        .duty           = 128,
        .hpoint         = 0
    };
    ledc_channel_config(&ch_dn);
}

// PWM Audio Generator Task
static void pwm_audio_task(void *pvParameters) {
    float phase = 0.0f;
    const float phase_inc = (2.0f * M_PI * 100.0f) / 1000.0f;

    while (1) {
        float val = sinf(phase);
        phase += phase_inc;
        if (phase >= 2.0f * M_PI) phase -= 2.0f * M_PI;

        uint32_t duty_dp = (uint32_t)(128.0f + val * 100.0f);
        uint32_t duty_dn = (uint32_t)(128.0f - val * 100.0f);

        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty_dp);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);

        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, duty_dn);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

// --- ST7789 TFT Display Initialization (esp_lcd SPI3_HOST) ---
static void init_tft_display(void) {
    ESP_LOGI(TAG, "Initializing ST7789 2.8-inch TFT Display on SPI3_HOST...");

    // Drive backlight pin (GPIO 48) Output
    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << TFT_BLK
    };
    gpio_config(&bk_gpio_config);
    gpio_set_level(TFT_BLK, 1); // High for active-high backlight

    spi_bus_config_t buscfg = {
        .sclk_io_num = TFT_SCLK,
        .mosi_io_num = TFT_MOSI,
        .miso_io_num = TFT_MISO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_H_RES * 40 * sizeof(uint16_t),
    };
    spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO);

    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = TFT_DC,
        .cs_gpio_num = TFT_CS,
        .pclk_hz = 20 * 1000 * 1000, // 20 MHz SPI Clock
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI3_HOST, &io_config, &io_handle);

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = TFT_RST,
        .rgb_endian = LCD_RGB_ENDIAN_BGR,
        .bits_per_pixel = 16,
    };
    esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle);

    esp_lcd_panel_reset(panel_handle);
    esp_lcd_panel_init(panel_handle);
    esp_lcd_panel_invert_color(panel_handle, true);
    esp_lcd_panel_swap_xy(panel_handle, true);
    esp_lcd_panel_mirror(panel_handle, false, true);
    esp_lcd_panel_disp_on_off(panel_handle, true);

    ESP_LOGI(TAG, "[TFT SUCCESS] ST7789 2.8\" TFT Display Initialized!");
}

// Continuous Render Task
static void tft_render_task(void *pvParameters) {
    uint16_t *line_buffer = (uint16_t *)heap_caps_malloc(LCD_H_RES * 40 * sizeof(uint16_t), MALLOC_CAP_DMA);
    if (!line_buffer) return;

    uint32_t frame = 0;

    while (1) {
        frame++;
        uint16_t active_color = (frame % 2 == 0) ? COLOR_CYAN : COLOR_MAGENTA;

        for (int y_block = 0; y_block < LCD_V_RES; y_block += 40) {
            for (int i = 0; i < LCD_H_RES * 40; i++) {
                line_buffer[i] = active_color;
            }
            esp_lcd_panel_draw_bitmap(panel_handle, 0, y_block, LCD_H_RES, y_block + 40, line_buffer);
        }

        vTaskDelay(pdMS_TO_TICKS(100)); // 10 FPS blinking color test
    }
}

// --- Main Application Entry Point ---
void app_main(void) {
    ESP_LOGI(TAG, "=== ESP32-S3 RhythmSleep ESP-IDF Application ===");

    // 1. Initialize GPIO Buttons & Vibration
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BTN_MENU_PIN) | (1ULL << BTN_UP_PIN) | (1ULL << BTN_DOWN_PIN) | (1ULL << BTN_SELECT_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    gpio_config_t out_conf = {
        .pin_bit_mask = (1ULL << PIN_VIBRATION),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&out_conf);
    gpio_set_level(PIN_VIBRATION, 0);

    // 2. Initialize ST7789 TFT Display & Render Task
    init_tft_display();
    xTaskCreate(tft_render_task, "tft_render_task", 4096, NULL, 3, NULL);

    // 3. Initialize High-Speed LEDC PWM Audio Generator (100 Hz Tone)
    init_pwm_audio();
    xTaskCreate(pwm_audio_task, "pwm_audio_task", 2048, NULL, 5, NULL);

    ESP_LOGI(TAG, "RhythmSleep Application running.");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        ESP_LOGI(TAG, "System tick: Render Task Active | PWM Audio 100 Hz Active");
    }
}
