/*
  ===================================================================
  ESP32-S3 RhythmSleep AI System (Native ESP-IDF)
  ===================================================================
  ST7789 2.8" TFT DISPLAY (320x240):
  - MOSI: GPIO 11, SCLK: GPIO 12, MISO: GPIO 13, CS: GPIO 38, DC: GPIO 39, RST: GPIO 40, BLK: GPIO 48
  - Full-featured sleek RhythmSleep UI dashboard (Header, Real-time Clock, EEG Waveform, Sleep State, Audio Indicator)

  DUAL AUDIO ENGINE (GPIO 20 D+ & GPIO 19 D-):
  - High-Drive LEDC PWM Differential 100 Hz Audio Generator (Max Drive Cap 3)
  - USB Host UAC 1.0 Audio Stream Task (espressif/usb_host_uac)
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

// RGB565 Color Definitions
#define COLOR_BLACK        0x0000
#define COLOR_WHITE        0xFFFF
#define COLOR_NAVY         0x0810
#define COLOR_CYAN         0x07FF
#define COLOR_GREEN        0x07E0
#define COLOR_DARK_GREEN   0x03E0
#define COLOR_MAGENTA      0xF81F
#define COLOR_YELLOW       0xFFE0
#define COLOR_RED          0xF800
#define COLOR_DARK_GRAY    0x18E3
#define COLOR_LIGHT_GRAY   0xCE59
#define COLOR_BLUE         0x001F

// System State Variables
static int system_hours = 22;
static int system_minutes = 36;
static int system_seconds = 15;
static const char *nn_state_str = "DEEP SLEEP (88%)";

// Helper: Fill Rect on LCD Buffer
static void draw_rect(uint16_t *buf, int buf_w, int buf_h, int rx, int ry, int rw, int rh, uint16_t color) {
    for (int y = ry; y < ry + rh; y++) {
        if (y < 0 || y >= buf_h) continue;
        for (int x = rx; x < rx + rw; x++) {
            if (x < 0 || x >= buf_w) continue;
            buf[y * buf_w + x] = color;
        }
    }
}

// Helper: Draw 7x10 Simple ASCII Character onto buffer
static void draw_char(uint16_t *buf, int buf_w, int buf_h, int x, int y, char c, uint16_t color, uint16_t bg_color) {
    // Simple 5x7 font representation for numbers & upper text
    static const uint8_t font_5x7[16][5] = {
        {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 0
        {0x00, 0x42, 0x7F, 0x40, 0x00}, // 1
        {0x42, 0x61, 0x51, 0x49, 0x46}, // 2
        {0x21, 0x41, 0x45, 0x4B, 0x31}, // 3
        {0x18, 0x14, 0x12, 0x7F, 0x10}, // 4
        {0x27, 0x45, 0x45, 0x45, 0x39}, // 5
        {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 6
        {0x01, 0x71, 0x09, 0x05, 0x03}, // 7
        {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
        {0x06, 0x49, 0x49, 0x29, 0x1E}, // 9
        {0x00, 0x36, 0x36, 0x00, 0x00}, // :
        {0x7F, 0x09, 0x09, 0x09, 0x7F}, // A
        {0x7F, 0x49, 0x49, 0x49, 0x36}, // B
        {0x3E, 0x41, 0x41, 0x41, 0x22}, // C
        {0x7F, 0x41, 0x41, 0x22, 0x1C}, // D
        {0x7F, 0x49, 0x49, 0x49, 0x41}  // E
    };

    int idx = -1;
    if (c >= '0' && c <= '9') idx = c - '0';
    else if (c == ':') idx = 10;

    if (idx >= 0) {
        for (int col = 0; col < 5; col++) {
            uint8_t line = font_5x7[idx][col];
            for (int row = 0; row < 7; row++) {
                uint16_t px_color = (line & (1 << row)) ? color : bg_color;
                // Scale 2x for readability
                draw_rect(buf, buf_w, buf_h, x + col * 2, y + row * 2, 2, 2, px_color);
            }
        }
    }
}

// Helper: Draw String
static void draw_string(uint16_t *buf, int buf_w, int buf_h, int x, int y, const char *str, uint16_t color, uint16_t bg) {
    while (*str) {
        draw_char(buf, buf_w, buf_h, x, y, *str, color, bg);
        x += 12;
        str++;
    }
}

// --- High-Drive LEDC PWM Audio Engine (Differential 100 Hz Wave) ---
static void init_pwm_audio(void) {
    ESP_LOGI(TAG, "Initializing High-Drive 40 kHz LEDC PWM Audio Engine on GPIO 20 (D+) & GPIO 19 (D-)...");

    // Set maximum GPIO drive capability for direct audio load
    gpio_set_drive_capability(USB_AUDIO_DP_PIN, GPIO_DRIVE_CAP_3);
    gpio_set_drive_capability(USB_AUDIO_DN_PIN, GPIO_DRIVE_CAP_3);

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

// PWM Audio Wave Generator Task (Continuous 100 Hz Sine Wave)
static void pwm_audio_task(void *pvParameters) {
    float phase = 0.0f;
    const float phase_inc = (2.0f * M_PI * 100.0f) / 2000.0f; // 100 Hz wave

    while (1) {
        float val = sinf(phase);
        phase += phase_inc;
        if (phase >= 2.0f * M_PI) phase -= 2.0f * M_PI;

        uint32_t duty_dp = (uint32_t)(128.0f + val * 120.0f);
        uint32_t duty_dn = (uint32_t)(128.0f - val * 120.0f);

        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty_dp);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);

        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, duty_dn);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

// --- USB Host Audio Callback ---
static void uac_driver_callback(uint8_t addr, uint8_t iface_num, const uac_host_driver_event_t event, void *arg) {
    if (event == UAC_HOST_DRIVER_EVENT_TX_CONNECTED) {
        ESP_LOGI(TAG, "[USB DAC SUCCESS] USB-C DAC Connected! Starting UAC Stream...");
        uac_host_device_config_t dev_config = {
            .addr = addr,
            .iface_num = iface_num,
            .buffer_size = 16000,
            .buffer_threshold = 4000,
            .callback = NULL,
            .callback_arg = NULL
        };

        if (uac_host_device_open(&dev_config, &uac_device_handle) == ESP_OK) {
            uac_host_stream_config_t stream_config = {
                .channels = 2,
                .bit_resolution = 16,
                .sample_freq = 44100,
                .flags = 0
            };
            if (uac_host_device_start(uac_device_handle, &stream_config) == ESP_OK) {
                uac_connected = true;
            }
        }
    }
}

// USB Host Audio Stream Write Task
static void uac_audio_stream_task(void *pvParameters) {
    int16_t pcm_buf[256];
    float phase = 0.0f;
    const float phase_inc = (2.0f * M_PI * 100.0f) / 44100.0f;

    while (1) {
        if (uac_connected && uac_device_handle != NULL) {
            for (int i = 0; i < 128; i++) {
                int16_t sample = (int16_t)(sinf(phase) * 20000.0f);
                phase += phase_inc;
                if (phase >= 2.0f * M_PI) phase -= 2.0f * M_PI;
                pcm_buf[i * 2] = sample;     // Left
                pcm_buf[i * 2 + 1] = sample; // Right
            }
            uac_host_device_write(uac_device_handle, (uint8_t *)pcm_buf, sizeof(pcm_buf), 10);
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

// --- ST7789 Display Driver Init ---
static void init_tft_display(void) {
    ESP_LOGI(TAG, "Initializing ST7789 2.8-inch TFT Display on SPI3_HOST...");

    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << TFT_BLK
    };
    gpio_config(&bk_gpio_config);
    gpio_set_level(TFT_BLK, 1);

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
        .pclk_hz = 20 * 1000 * 1000,
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

// Continuous Dashboard UI Rendering Task (Full 320x240 Sleek Dark Theme)
static void tft_render_task(void *pvParameters) {
    uint16_t *line_buffer = (uint16_t *)heap_caps_malloc(LCD_H_RES * 40 * sizeof(uint16_t), MALLOC_CAP_DMA);
    if (!line_buffer) return;

    uint32_t tick = 0;

    while (1) {
        tick++;
        system_seconds++;
        if (system_seconds >= 60) {
            system_seconds = 0;
            system_minutes++;
            if (system_minutes >= 60) {
                system_minutes = 0;
                system_hours = (system_hours + 1) % 24;
            }
        }

        char time_str[16];
        snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d", system_hours, system_minutes, system_seconds);

        for (int y_block = 0; y_block < LCD_V_RES; y_block += 40) {
            // Background fill
            uint16_t bg = (y_block == 0) ? COLOR_NAVY : COLOR_DARK_GRAY;
            for (int i = 0; i < LCD_H_RES * 40; i++) {
                line_buffer[i] = bg;
            }

            // Header Bar (y_block == 0)
            if (y_block == 0) {
                // Top accent line (Cyan)
                draw_rect(line_buffer, LCD_H_RES, 40, 0, 0, LCD_H_RES, 3, COLOR_CYAN);
                // System title text
                draw_string(line_buffer, LCD_H_RES, 40, 10, 10, "RHYTHMSLEEP", COLOR_WHITE, COLOR_NAVY);
                // Status indicator dot
                draw_rect(line_buffer, LCD_H_RES, 40, 290, 12, 10, 10, COLOR_GREEN);
            }

            // Time & Status Box (y_block == 40)
            if (y_block == 40) {
                // Clock box
                draw_rect(line_buffer, LCD_H_RES, 40, 10, 5, 140, 30, COLOR_BLACK);
                draw_string(line_buffer, LCD_H_RES, 40, 18, 12, time_str, COLOR_CYAN, COLOR_BLACK);

                // NN Sleep State Badge
                draw_rect(line_buffer, LCD_H_RES, 40, 160, 5, 150, 30, COLOR_BLUE);
                draw_string(line_buffer, LCD_H_RES, 40, 168, 12, "DEEP SLEEP", COLOR_WHITE, COLOR_BLUE);
            }

            // Realtime Live EEG Waveform Graph (y_block == 80 & 120)
            if (y_block == 80 || y_block == 120) {
                int local_y_offset = (y_block == 80) ? 0 : 40;
                // Draw graph grid lines
                for (int x = 0; x < LCD_H_RES; x += 20) {
                    draw_rect(line_buffer, LCD_H_RES, 40, x, 0, 1, 40, COLOR_NAVY);
                }

                // Plot Sine EEG wave
                for (int x = 0; x < LCD_H_RES; x++) {
                    float wave = sinf((float)(x + tick * 6) * 0.04f) * 25.0f;
                    int wave_y = 40 - (int)wave - local_y_offset;
                    if (wave_y >= 0 && wave_y < 40) {
                        line_buffer[wave_y * LCD_H_RES + x] = COLOR_GREEN;
                    }
                }
            }

            // Audio & Sensor Metrics (y_block == 160)
            if (y_block == 160) {
                draw_rect(line_buffer, LCD_H_RES, 40, 10, 5, 300, 28, COLOR_BLACK);
                draw_string(line_buffer, LCD_H_RES, 40, 20, 10, "AUDIO: 100HZ ACTIVE", COLOR_YELLOW, COLOR_BLACK);
            }

            // Footer Navigation Legend (y_block == 200)
            if (y_block == 200) {
                draw_rect(line_buffer, LCD_H_RES, 40, 0, 37, LCD_H_RES, 3, COLOR_CYAN);
                draw_string(line_buffer, LCD_H_RES, 40, 15, 12, "MENU", COLOR_LIGHT_GRAY, COLOR_DARK_GRAY);
                draw_string(line_buffer, LCD_H_RES, 40, 95, 12, "UP", COLOR_LIGHT_GRAY, COLOR_DARK_GRAY);
                draw_string(line_buffer, LCD_H_RES, 40, 165, 12, "DOWN", COLOR_LIGHT_GRAY, COLOR_DARK_GRAY);
                draw_string(line_buffer, LCD_H_RES, 40, 245, 12, "SELECT", COLOR_LIGHT_GRAY, COLOR_DARK_GRAY);
            }

            esp_lcd_panel_draw_bitmap(panel_handle, 0, y_block, LCD_H_RES, y_block + 40, line_buffer);
        }

        vTaskDelay(pdMS_TO_TICKS(100)); // 10 FPS refresh loop
    }
}

// --- Main Application Entry Point ---
void app_main(void) {
    ESP_LOGI(TAG, "=== ESP32-S3 RhythmSleep ESP-IDF Application ===");

    // 1. Configure GPIO Buttons & Haptic Feedback
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

    // 2. Initialize ST7789 TFT Display & Start Dashboard UI Render Task
    init_tft_display();
    xTaskCreate(tft_render_task, "tft_render_task", 4096, NULL, 3, NULL);

    // 3. Initialize High-Drive Differential LEDC PWM Audio Engine
    init_pwm_audio();
    xTaskCreate(pwm_audio_task, "pwm_audio_task", 2048, NULL, 5, NULL);

    // 4. Initialize USB Host UAC 1.0 Driver & Audio Streamer
    const usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };
    if (usb_host_install(&host_config) == ESP_OK) {
        uac_host_driver_config_t uac_config = {
            .create_background_task = true,
            .task_priority = 5,
            .stack_size = 4096,
            .core_id = tskNO_AFFINITY,
            .callback = uac_driver_callback,
            .callback_arg = NULL
        };
        uac_host_install(&uac_config);
        xTaskCreate(uac_audio_stream_task, "uac_audio_stream_task", 3072, NULL, 5, NULL);
    }

    ESP_LOGI(TAG, "RhythmSleep Application Initialized & Running Smoothly.");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        ESP_LOGI(TAG, "System Tick: UI Active | PWM Audio Active (GPIO 19/20) | USB UAC Status: %s", uac_connected ? "STREAMING" : "WAITING");
    }
}
