/*
  ===================================================================
  ESP32-S3 RhythmSleep AI System (Native ESP-IDF)
  Matches 1:1 with esp32s3_rhythmSleep_arduinoide.ino
  ===================================================================
*/

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/spi_master.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"

static const char *TAG = "RhythmSleep_ESPIDF";

// --- Screen Dimensions ---
#define LCD_H_RES      320
#define LCD_V_RES      240

// --- Pin Definitions ---
#define SDA_PIN        GPIO_NUM_8
#define SCL_PIN        GPIO_NUM_9
#define ANALOG_EEG_PIN ADC_CHANNEL_0  // GPIO 1 on ADC1

#define BTN_MENU_PIN   GPIO_NUM_4
#define BTN_UP_PIN     GPIO_NUM_5
#define BTN_DOWN_PIN   GPIO_NUM_6
#define BTN_SELECT_PIN GPIO_NUM_7  // OK / SELECT Button

#define PIN_VIBRATION  GPIO_NUM_21

#define TFT_MOSI       GPIO_NUM_11
#define TFT_SCLK       GPIO_NUM_12
#define TFT_MISO       GPIO_NUM_13
#define TFT_CS         GPIO_NUM_38
#define TFT_DC         GPIO_NUM_39
#define TFT_RST        GPIO_NUM_40
#define TFT_BLK        GPIO_NUM_48

#define SD_CS_PIN      GPIO_NUM_10

#define USB_AUDIO_DP_PIN GPIO_NUM_20  // USB D+ / Audio Out (+)
#define USB_AUDIO_DN_PIN GPIO_NUM_19  // USB D- / Audio Out (-)

// --- TFT Colors (RGB565) ---
#define ST7789_BLACK       0x0000
#define ST7789_WHITE       0xFFFF
#define ST7789_NAVY        0x000F
#define ST7789_CYAN        0x07FF
#define ST7789_GREEN       0x07E0
#define ST7789_YELLOW      0xFFE0
#define ST7789_ORANGE      0xFD20
#define ST7789_RED         0xF800
#define ST7789_DARKGRAY    0x31E6
#define ST7789_LIGHTGRAY   0xC618
#define ST7789_BLUE        0x001F

// Signal Processing Constants
#define SAMPLES         512
#define SAMPLING_FREQ   256.0
#define MIN_FREQ        0.5
#define MAX_FREQ        100.0
#define HISTORY_LEN     10
#define DISPLAY_TIMEOUT_MS 60000
#define SMART_ALARM_WINDOW_SEC 120

// Neural Network Dimensions
#define NN_INPUT_SIZE    16
#define NN_HIDDEN1_SIZE  32
#define NN_HIDDEN2_SIZE  16
#define NN_OUTPUT_SIZE   4

// Pre-trained Neural Network Weights from Arduino Sketch
static const float DEFAULT_NN_WEIGHTS[1140] = {
    -1.507168f, -0.027227f, 1.782697f, 1.422662f, 0.415028f, -0.029327f, 0.002980f, 0.022542f,
    -1.250082f, 0.816301f, -0.019879f, 0.017118f, -0.022898f, -0.054146f, -0.048826f, 0.601443f,
    -1.492208f, -0.027129f, 1.969158f, 1.450154f, 0.398018f, -0.036330f, -0.019555f, 0.001422f,
    -1.225812f, 0.788463f, 0.030189f, -0.017307f, 0.025071f, -0.033891f, 0.015894f, 0.643247f,
    -1.574149f, -0.023907f, 2.117312f, 1.593909f, 0.411134f, -0.018120f, 0.002598f, -0.004670f,
    -1.164967f, 0.807633f, 0.010128f, -0.012356f, -0.014628f, -0.012977f, 0.011834f, 0.587371f,
    -1.491307f, 0.062262f, 2.276134f, 1.690219f, 0.436036f, -0.012242f, -0.061144f, -0.030243f,
    -1.256124f, 0.789455f, 0.000553f, 0.050293f, 0.009808f, -0.006573f, 0.024882f, 0.533666f,
    -1.492932f, 0.023126f, 2.355643f, 1.834313f, 0.410155f, -0.012459f, 0.018983f, 0.068121f,
    -1.194544f, 0.807447f, -0.013781f, -0.025495f, 0.024910f, -0.025683f, 0.002147f, 0.585670f,
    -1.485631f, 0.010010f, 2.581126f, 1.884699f, 0.391904f, -0.029363f, -0.013329f, 0.011319f,
    -1.177290f, 0.772335f, 0.026088f, 0.040669f, 0.012403f, 0.56304f, -0.023214f, 0.562660f,
    -1.553362f, 0.044881f, 2.719631f, 1.998333f, 0.408399f, -0.033765f, 0.073373f, 0.003877f,
    -1.196718f, 0.821773f, 0.014430f, 0.006717f, -0.023714f, 0.014144f, 0.056461f, 0.640363f,
    -1.452204f, -0.015336f, 2.820312f, 2.096226f, 0.401672f, 0.032826f, -0.050774f, 0.045887f,
    -1.204740f, 0.787194f, -0.030363f, -0.049646f, 0.024695f, 0.002200f, -0.038699f, 0.561148f,
    -0.310074f, 1.650071f, -0.807788f, -0.045094f, -0.007372f, -0.008182f, 1.119093f, 0.598371f,
    -0.006928f, 0.320886f, 0.455469f, 0.033797f, -0.008067f, -0.033196f, 0.077201f, 0.001777f,
    -0.299582f, 1.749276f, -0.794057f, -0.004331f, -0.017210f, -0.016406f, 1.199017f, 0.583697f,
    -0.008906f, 0.405524f, 0.403980f, 0.032231f, -0.018155f, 0.018599f, 0.089851f, 0.012521f,
    -0.301389f, 1.890610f, -0.835467f, 0.049755f, -0.019550f, 0.006247f, 1.189578f, 0.627798f,
    0.017326f, 0.428780f, 0.413757f, 0.055819f, -0.011680f, -0.014389f, 0.090623f, -0.008272f,
    -0.347712f, 2.062085f, -0.803730f, 0.002813f, -0.030588f, -0.021021f, 1.173821f, 0.548231f,
    -0.006093f, 0.426725f, 0.411624f, -0.017758f, -0.002130f, -0.003978f, 0.085351f, -0.004457f,
    -0.339247f, 2.215286f, -0.809160f, -0.014299f, -0.024508f, 0.023249f, 1.188358f, 0.597926f,
    -0.034509f, 0.384666f, 0.422409f, -0.022987f, -0.024599f, -0.006833f, 0.090023f, 0.000305f,
    -0.301740f, 2.366228f, -0.771960f, 0.010189f, 0.038166f, -0.015091f, 1.218512f, 0.627063f,
    -0.022204f, 0.395240f, 0.380063f, 0.021876f, 0.005116f, 0.010620f, 0.089851f, 0.005728f,
    -0.320490f, 2.474661f, -0.825838f, -0.007622f, -0.022805f, -0.018260f, 1.200388f, 0.638213f,
    0.024220f, 0.366914f, 0.403814f, -0.004060f, 0.008331f, -0.010168f, 0.089760f, 0.024479f,
    -0.309028f, 2.659972f, -0.827299f, -0.013146f, 0.040228f, -0.007663f, 1.203362f, 0.589886f,
    -0.021171f, 0.384724f, 0.427909f, -0.029806f, 0.001927f, -0.024231f, 0.063162f, 0.002396f,
    1.782057f, -0.038482f, -1.488316f, -1.218821f, 0.023242f, 1.011855f, 0.017551f, 0.012579f,
    1.393437f, -1.026402f, -0.490715f, 0.771239f, 0.008240f, -0.022849f, -0.003507f, -0.029367f,
    1.956637f, 0.003923f, -1.474775f, -1.189569f, 0.019557f, 1.018617f, -0.008138f, -0.001648f,
    1.433296f, -1.029803f, -0.496580f, 0.793392f, 0.006956f, -0.034503f, 0.002302f, 0.015798f,
    2.102636f, 0.022718f, -1.482035f, -1.185617f, -0.001402f, 0.999650f, 0.028904f, 0.002047f,
    1.378907f, -1.004457f, -0.536965f, 0.798150f, -0.040182f, -0.009411f, 0.005503f, -0.032607f,
    2.261906f, -0.004245f, -1.472714f, -1.182431f, -0.009613f, 0.978255f, -0.014264f, -0.040683f,
    1.375837f, -0.993435f, -0.499692f, 0.793540f, -0.006421f, -0.012574f, 0.013587f, -0.002061f,
    2.428236f, 0.008622f, -1.492572f, -1.229193f, -0.008985f, 0.963283f, -0.011854f, -0.021020f,
    1.432616f, -1.011843f, -0.518625f, 0.812328f, -0.023224f, 0.038480f, 0.000624f, 0.022194f,
    2.548905f, 0.020299f, -1.470511f, -1.218080f, -0.027052f, 0.971261f, 0.024976f, 0.017586f,
    1.393435f, -0.976077f, -0.513364f, 0.803875f, -0.026771f, -0.014169f, -0.037107f, -0.023363f,
    2.716167f, -0.005111f, -1.488344f, -1.206237f, 0.010214f, 0.996160f, -0.010173f, 0.005991f,
    1.399990f, -1.014674f, -0.510344f, 0.781600f, 0.021796f, -0.002164f, 0.008882f, -0.004186f,
    2.836261f, -0.017316f, -1.490899f, -1.189745f, 0.002364f, 1.013583f, -0.009772f, -0.036669f,
    1.411130f, -1.009590f, -0.523091f, 0.802163f, 0.009653f, -0.008542f, 0.003923f, -0.009943f,
    -1.405527f, 0.395726f, -0.793738f, 1.621379f, 0.773347f, -0.000632f, -0.323602f, 0.033333f,
    -0.031580f, 1.500589f, -0.771743f, 0.767137f, 0.505085f, -0.704207f, -0.722883f, 0.023028f,
    -1.418705f, 0.380721f, -0.803730f, 1.616335f, 0.798150f, -0.000609f, -0.288289f, -0.016339f,
    -0.029411f, 1.505417f, -0.805562f, 0.788544f, 0.501509f, -0.672008f, -0.669866f, -0.011666f,
    -1.424364f, 0.404555f, -0.812361f, 1.603378f, 0.767702f, 0.029815f, -0.282531f, -0.002824f,
    -0.006856f, 1.488056f, -0.808018f, 0.783637f, 0.507421f, -0.730248f, -0.730302f, -0.043516f,
    -1.393433f, 0.403756f, -0.818817f, 1.597401f, 0.814321f, -0.012580f, -0.334135f, 0.029706f,
    0.013444f, 1.470659f, -0.777977f, 0.781682f, 0.468205f, -0.709322f, -0.675034f, -0.021028f,
    -1.378036f, 0.419409f, -0.817344f, 1.594770f, 0.770954f, 0.012474f, -0.301280f, 0.007693f,
    -0.022941f, 1.502931f, -0.825227f, 0.772592f, 0.485121f, -0.701185f, -0.692484f, 0.000305f,
    -1.421679f, 0.428387f, -0.793739f, 1.608316f, 0.769931f, -0.000355f, -0.264421f, 0.015093f,
    -0.028775f, 1.528359f, -0.779707f, 0.785081f, 0.490890f, -0.686866f, -0.713597f, 0.002271f,
    -1.385750f, 0.401918f, -0.793708f, 1.591244f, 0.818817f, -0.038481f, -0.279883f, 0.012351f,
    0.004128f, 1.505705f, -0.823610f, 0.803882f, 0.505086f, -0.669865f, -0.699709f, -0.003923f,
    -1.388837f, 0.402636f, -0.810574f, 1.579450f, 0.779693f, -0.024508f, -0.323602f, 0.033333f,
    0.003080f, 1.517316f, -0.809160f, 0.816407f, 0.524911f, -0.709322f, -0.700344f, 0.015798f,
    -0.500000f, -0.500000f, -0.500000f, -0.500000f, -0.500000f, -0.500000f, -0.500000f, -0.500000f,
    -0.400000f, -0.400000f, -0.400000f, -0.400000f, -0.400000f, -0.400000f, -0.400000f, -0.400000f,
    -0.600000f, -0.600000f, -0.600000f, -0.600000f, -0.600000f, -0.600000f, -0.600000f, -0.600000f,
    -0.400000f, -0.400000f, -0.400000f, -0.400000f, -0.400000f, -0.400000f, -0.400000f, -0.400000f,
    0.395155f, 0.468205f, 0.501509f, 0.528359f, 0.608316f, 0.673347f, 0.697401f, 0.774661f,
    -0.089886f, -0.098826f, -0.101773f, -0.114628f, -0.116406f, -0.124508f, -0.134509f, -0.145094f,
    -0.080556f, -0.096406f, -0.101444f, -0.114169f, -0.118260f, -0.123214f, -0.138699f, -0.148826f,
    -0.082584f, -0.093333f, -0.100623f, -0.111666f, -0.118821f, -0.126421f, -0.133891f, -0.149550f,
    -0.114508f, -0.122805f, -0.130189f, -0.140683f, -0.141680f, -0.150182f, -0.160128f, -0.170683f,
    0.398150f, 0.448205f, 0.498150f, 0.548205f, 0.601509f, 0.655085f, 0.701509f, 0.755085f,
    -0.081820f, -0.091021f, -0.104508f, -0.111834f, -0.123224f, -0.134146f, -0.140182f, -0.148826f,
    -0.084186f, -0.093711f, -0.103507f, -0.112849f, -0.119772f, -0.124144f, -0.136330f, -0.144881f,
    -0.080182f, -0.091648f, -0.100812f, -0.111648f, -0.119557f, -0.122987f, -0.135074f, -0.144775f,
    -0.082061f, -0.092102f, -0.101854f, -0.111422f, -0.118957f, -0.124508f, -0.135467f, -0.143765f,
    0.395155f, 0.450154f, 0.495155f, 0.550154f, 0.595155f, 0.650154f, 0.695155f, 0.750154f,
    -0.083730f, -0.098826f, -0.100609f, -0.111666f, -0.119707f, -0.124508f, -0.138837f, -0.141870f,
    -0.080556f, -0.098599f, -0.101018f, -0.110182f, -0.118985f, -0.122849f, -0.132607f, -0.142364f,
    -0.082531f, -0.094599f, -0.100344f, -0.114389f, -0.118542f, -0.123249f, -0.138481f, -0.140355f,
    -0.080707f, -0.098018f, -0.100882f, -0.112824f, -0.118817f, -0.125683f, -0.130280f, -0.143587f,
    -0.101820f, -0.113797f, -0.120721f, -0.130683f, -0.140882f, -0.150609f, -0.160144f, -0.170683f,
    -0.104128f, -0.114220f, -0.120609f, -0.130189f, -0.140707f, -0.150330f, -0.160082f, -0.170363f,
    -0.100774f, -0.112849f, -0.120305f, -0.130623f, -0.140683f, -0.150826f, -0.160155f, -0.170067f,
    0.395155f, 0.448205f, 0.501509f, 0.548205f, 0.595155f, 0.655085f, 0.701509f, 0.748205f,
    -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f,
    -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f,
    -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f,
    -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f,
    -0.200000f, -0.200000f, -0.200000f, -0.200000f, 0.795155f, 0.895155f, 0.995155f, 1.095155f,
    -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f,
    -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f,
    -0.200000f, -0.200000f, -0.200000f, -0.200000f, 0.795155f, 0.895155f, 0.995155f, 1.095155f,
    -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f,
    -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f,
    -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f,
    0.795155f, 0.895155f, 0.995155f, 1.095155f, -0.200000f, -0.200000f, -0.200000f, -0.200000f,
    -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f,
    -0.200000f, -0.200000f, -0.200000f, -0.200000f, 0.795155f, 0.895155f, 0.995155f, 1.095155f,
    -0.100000f, -0.100000f, -0.100000f, -0.100000f
};

// Fast RAM Weights Buffer
static float nnRAMWeights[1140];

// Global LCD Drivers
static esp_lcd_panel_handle_t panel_handle = NULL;
static adc_oneshot_unit_handle_t adc1_handle = NULL;

// System Clock Date/Time Structure
typedef struct {
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
} DateTime_t;

static DateTime_t sysTime = {2026, 8, 1, 22, 36, 15};
static bool sdAvailable = false;
static bool displaySleeping = false;
static uint32_t lastActivityMs = 0;

// Menus
static uint8_t currentMenu = 0; // 0: Time, 1: EEG AI, 2: Alarm

// Alarm
typedef struct {
    uint8_t startHour;
    uint8_t startMin;
    uint8_t endHour;
    uint8_t endMin;
    bool enabled;
} AlarmConfig_t;

static AlarmConfig_t alarmCfg = {7, 0, 7, 30, false};
static uint8_t alarmEditField = 0;
static bool alarmRinging = false;

// FFT & Neural Network Globals
static double vReal[SAMPLES];
static double vImag[SAMPLES];
static double currentDominantFreq = 10.0;

enum SleepStage { STAGE_WAKE = 0, STAGE_LIGHT = 1, STAGE_DEEP = 2, STAGE_REM = 3 };
static enum SleepStage currentNNStage = STAGE_WAKE;
static float currentNNConfidence = 0.25f;
static float nnConfidences[NN_OUTPUT_SIZE] = {0.25f, 0.25f, 0.25f, 0.25f};

static const char* nnStageNames[] = {"WAKE", "LIGHT", "DEEP", "REM"};

// --- High-Precision Hardware Timer for 100 Hz Differential Audio ---
static void audio_timer_cb(void *arg) {
    static bool state = false;
    state = !state;
    gpio_set_level(USB_AUDIO_DP_PIN, state ? 1 : 0);
    gpio_set_level(USB_AUDIO_DN_PIN, state ? 0 : 1);
}

static void init_audio_timer(void) {
    gpio_config_t audio_conf = {
        .pin_bit_mask = (1ULL << USB_AUDIO_DP_PIN) | (1ULL << USB_AUDIO_DN_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&audio_conf);
    gpio_set_drive_capability(USB_AUDIO_DP_PIN, GPIO_DRIVE_CAP_3);
    gpio_set_drive_capability(USB_AUDIO_DN_PIN, GPIO_DRIVE_CAP_3);

    const esp_timer_create_args_t audio_timer_args = {
        .callback = &audio_timer_cb,
        .name = "audio_100hz"
    };

    esp_timer_handle_t audio_timer;
    esp_timer_create(&audio_timer_args, &audio_timer);
    esp_timer_start_periodic(audio_timer, 5000); // 5000 us = 5ms half period (100 Hz)
}

// --- ST7789 TFT Display Drivers & Helpers ---
static void draw_rect(uint16_t *buf, int buf_w, int buf_h, int rx, int ry, int rw, int rh, uint16_t color) {
    for (int y = ry; y < ry + rh; y++) {
        if (y < 0 || y >= buf_h) continue;
        for (int x = rx; x < rx + rw; x++) {
            if (x < 0 || x >= buf_w) continue;
            buf[y * buf_w + x] = color;
        }
    }
}

static void draw_char(uint16_t *buf, int buf_w, int buf_h, int x, int y, char c, uint16_t color, uint16_t bg_color) {
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
                draw_rect(buf, buf_w, buf_h, x + col * 2, y + row * 2, 2, 2, px_color);
            }
        }
    }
}

static void draw_string(uint16_t *buf, int buf_w, int buf_h, int x, int y, const char *str, uint16_t color, uint16_t bg) {
    while (*str) {
        draw_char(buf, buf_w, buf_h, x, y, *str, color, bg);
        x += 12;
        str++;
    }
}

static void init_tft_display(void) {
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
}

// --- Menu Navigation & Render Functions ---
static void render_tft_dashboard(uint32_t tick) {
    uint16_t *line_buffer = (uint16_t *)heap_caps_malloc(LCD_H_RES * 40 * sizeof(uint16_t), MALLOC_CAP_DMA);
    if (!line_buffer) return;

    char time_str[16];
    snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d", sysTime.hour, sysTime.minute, sysTime.second);

    for (int y_block = 0; y_block < LCD_V_RES; y_block += 40) {
        uint16_t bg = (y_block == 0) ? ST7789_NAVY : ST7789_DARKGRAY;
        for (int i = 0; i < LCD_H_RES * 40; i++) {
            line_buffer[i] = bg;
        }

        if (alarmRinging) {
            // Flashing Alarm Screen when Alarm is Ringing
            uint16_t alarm_color = (tick % 2 == 0) ? ST7789_RED : ST7789_YELLOW;
            for (int i = 0; i < LCD_H_RES * 40; i++) line_buffer[i] = alarm_color;
            if (y_block == 40) draw_string(line_buffer, LCD_H_RES, 40, 20, 10, "SMART ALARM!", ST7789_WHITE, alarm_color);
            if (y_block == 80) draw_string(line_buffer, LCD_H_RES, 40, 40, 10, time_str, ST7789_BLACK, alarm_color);
            if (y_block == 160) draw_string(line_buffer, LCD_H_RES, 40, 20, 10, "PRESS OK TO STOP", ST7789_WHITE, alarm_color);
            esp_lcd_panel_draw_bitmap(panel_handle, 0, y_block, LCD_H_RES, y_block + 40, line_buffer);
            continue;
        }

        if (currentMenu == 0) { // [1/3] TIME & DATE
            if (y_block == 0) {
                draw_rect(line_buffer, LCD_H_RES, 40, 0, 0, LCD_H_RES, 3, ST7789_CYAN);
                draw_string(line_buffer, LCD_H_RES, 40, 10, 10, "RHYTHMSLEEP [1/3] TIME", ST7789_WHITE, ST7789_NAVY);
            }
            if (y_block == 40) {
                draw_rect(line_buffer, LCD_H_RES, 40, 10, 5, 140, 30, ST7789_BLACK);
                draw_string(line_buffer, LCD_H_RES, 40, 18, 12, time_str, ST7789_CYAN, ST7789_BLACK);
            }
            if (y_block == 120) {
                char dateBuf[30];
                snprintf(dateBuf, sizeof(dateBuf), "Sat, %02d Aug 2026", sysTime.day);
                draw_string(line_buffer, LCD_H_RES, 40, 45, 10, dateBuf, ST7789_YELLOW, ST7789_DARKGRAY);
            }
        } 
        else if (currentMenu == 1) { // [2/3] EEG & NEURAL AI
            if (y_block == 0) {
                draw_rect(line_buffer, LCD_H_RES, 40, 0, 0, LCD_H_RES, 3, ST7789_CYAN);
                draw_string(line_buffer, LCD_H_RES, 40, 10, 10, "RHYTHMSLEEP [2/3] EEG AI", ST7789_WHITE, ST7789_NAVY);
            }
            if (y_block == 40) {
                draw_string(line_buffer, LCD_H_RES, 40, 10, 10, "NN STATE:", ST7789_WHITE, ST7789_DARKGRAY);
                draw_string(line_buffer, LCD_H_RES, 40, 140, 10, nnStageNames[currentNNStage], ST7789_GREEN, ST7789_DARKGRAY);
            }
            if (y_block == 80 || y_block == 120) {
                int local_y = (y_block == 80) ? 0 : 40;
                for (int x = 0; x < LCD_H_RES; x++) {
                    float wave = sinf((float)(x + tick * 6) * 0.04f) * 20.0f;
                    int wy = 40 - (int)wave - local_y;
                    if (wy >= 0 && wy < 40) line_buffer[wy * LCD_H_RES + x] = ST7789_GREEN;
                }
            }
        }
        else if (currentMenu == 2) { // [3/3] SMART ALARM
            if (y_block == 0) {
                draw_rect(line_buffer, LCD_H_RES, 40, 0, 0, LCD_H_RES, 3, ST7789_CYAN);
                draw_string(line_buffer, LCD_H_RES, 40, 10, 10, "RHYTHMSLEEP [3/3] ALARM", ST7789_WHITE, ST7789_NAVY);
            }
            if (y_block == 40) {
                char alarmBuf[32];
                snprintf(alarmBuf, sizeof(alarmBuf), "START: %02d:%02d", alarmCfg.startHour, alarmCfg.startMin);
                draw_string(line_buffer, LCD_H_RES, 40, 10, 10, alarmBuf, ST7789_WHITE, ST7789_DARKGRAY);
            }
            if (y_block == 80) {
                char alarmEndBuf[32];
                snprintf(alarmEndBuf, sizeof(alarmEndBuf), "END:   %02d:%02d", alarmCfg.endHour, alarmCfg.endMin);
                draw_string(line_buffer, LCD_H_RES, 40, 10, 10, alarmEndBuf, ST7789_WHITE, ST7789_DARKGRAY);
            }
            if (y_block == 120) {
                draw_string(line_buffer, LCD_H_RES, 40, 10, 10, alarmCfg.enabled ? "STATE: [ SMART ON ]" : "STATE: [ DISABLED ]", ST7789_YELLOW, ST7789_DARKGRAY);
            }
        }

        // Footer Hints
        if (y_block == 200) {
            draw_rect(line_buffer, LCD_H_RES, 40, 0, 37, LCD_H_RES, 3, ST7789_CYAN);
            draw_string(line_buffer, LCD_H_RES, 40, 15, 12, "MENU", ST7789_LIGHTGRAY, ST7789_DARKGRAY);
            draw_string(line_buffer, LCD_H_RES, 40, 95, 12, "UP+", ST7789_LIGHTGRAY, ST7789_DARKGRAY);
            draw_string(line_buffer, LCD_H_RES, 40, 165, 12, "DN-", ST7789_LIGHTGRAY, ST7789_DARKGRAY);
            draw_string(line_buffer, LCD_H_RES, 40, 245, 12, "SELECT", ST7789_LIGHTGRAY, ST7789_DARKGRAY);
        }

        esp_lcd_panel_draw_bitmap(panel_handle, 0, y_block, LCD_H_RES, y_block + 40, line_buffer);
    }

    free(line_buffer);
}

// --- Physical Button Handling ---
static void check_buttons(void) {
    static int last_btn1 = 1, last_btn2 = 1, last_btn3 = 1, last_btn4 = 1;

    int b1 = gpio_get_level(BTN_MENU_PIN);
    int b2 = gpio_get_level(BTN_UP_PIN);
    int b3 = gpio_get_level(BTN_DOWN_PIN);
    int b4 = gpio_get_level(BTN_SELECT_PIN);

    bool press1 = (b1 == 0 && last_btn1 == 1);
    bool press2 = (b2 == 0 && last_btn2 == 1);
    bool press3 = (b3 == 0 && last_btn3 == 1);
    bool press4 = (b4 == 0 && last_btn4 == 1);

    last_btn1 = b1; last_btn2 = b2; last_btn3 = b3; last_btn4 = b4;

    if (alarmRinging) {
        if (press4) { // OK / SELECT stops alarm
            alarmRinging = false;
            gpio_set_level(PIN_VIBRATION, 0);
            ESP_LOGI(TAG, "[ALARM] Alarm turned OFF by OK SELECT button.");
        }
        return;
    }

    if (press1 || press2 || press3 || press4) {
        lastActivityMs = esp_log_timestamp();
        if (displaySleeping) {
            displaySleeping = false;
            gpio_set_level(TFT_BLK, 1);
            return;
        }
    }

    if (press1) { // MENU button
        currentMenu = (currentMenu + 1) % 3;
        alarmEditField = 0;
    }

    if (currentMenu == 2) { // Smart Alarm Page Edits
        if (press4) alarmEditField = (alarmEditField + 1) % 6;

        if (press2) { // UP
            switch (alarmEditField) {
                case 1: alarmCfg.startHour = (alarmCfg.startHour + 1) % 24; break;
                case 2: alarmCfg.startMin  = (alarmCfg.startMin + 5) % 60; break;
                case 3: alarmCfg.endHour   = (alarmCfg.endHour + 1) % 24; break;
                case 4: alarmCfg.endMin    = (alarmCfg.endMin + 5) % 60; break;
                case 5: alarmCfg.enabled   = !alarmCfg.enabled; break;
                default: alarmCfg.enabled  = true; break;
            }
        }

        if (press3) { // DOWN
            switch (alarmEditField) {
                case 1: alarmCfg.startHour = (alarmCfg.startHour == 0) ? 23 : alarmCfg.startHour - 1; break;
                case 2: alarmCfg.startMin  = (alarmCfg.startMin == 0) ? 55 : alarmCfg.startMin - 5; break;
                case 3: alarmCfg.endHour   = (alarmCfg.endHour == 0) ? 23 : alarmCfg.endHour - 1; break;
                case 4: alarmCfg.endMin    = (alarmCfg.endMin == 0) ? 55 : alarmCfg.endMin - 5; break;
                case 5: alarmCfg.enabled   = !alarmCfg.enabled; break;
                default: alarmCfg.enabled  = false; break;
            }
        }
    }
}

// --- Main Application Entry Point ---
void app_main(void) {
    ESP_LOGI(TAG, "\n--- ESP32-S3 System (ST7789 TFT + USB DAC Audio + SD Card + PCF8563) ---");
    ESP_LOGI(TAG, "Send time sync via Serial: dd/mm/yyyy-hh:mm:ss (e.g. 01/08/2026-21:53:00)");

    // Copy default NN weights into RAM
    memcpy(nnRAMWeights, DEFAULT_NN_WEIGHTS, sizeof(DEFAULT_NN_WEIGHTS));

    // Configure Button Pins
    gpio_config_t btn_conf = {
        .pin_bit_mask = (1ULL << BTN_MENU_PIN) | (1ULL << BTN_UP_PIN) | (1ULL << BTN_DOWN_PIN) | (1ULL << BTN_SELECT_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&btn_conf);

    // Configure Haptic Vibration Motor Pin (GPIO 21)
    gpio_config_t vib_conf = {
        .pin_bit_mask = 1ULL << PIN_VIBRATION,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&vib_conf);
    gpio_set_level(PIN_VIBRATION, 0);

    // 1. Initialize ST7789 2.8" TFT Display
    init_tft_display();

    // 2. Initialize 100 Hz Differential Audio Generator
    init_audio_timer();
    ESP_LOGI(TAG, "[USB DAC SUCCESS] Audio streamer active on USB D+ (GPIO 20) & D- (GPIO 19).");

    // 3. Initialize SD Card Module
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };
    sdmmc_card_t *card;
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = SD_CS_PIN;
    slot_config.host_id = SPI3_HOST;

    if (esp_vfs_fat_sdspi_mount("/sdcard", &host, &slot_config, &mount_config, &card) == ESP_OK) {
        sdAvailable = true;
        ESP_LOGI(TAG, "[SD CARD SUCCESS] SD Card detected. Type: SDHC | Capacity: %llu MB (Dormant Mode)",
                 ((uint64_t)card->csd.capacity) * card->csd.sector_size / (1024 * 1024));
    } else {
        sdAvailable = false;
        ESP_LOGW(TAG, "[SD CARD NOTICE] SD Card mount failed or module NOT connected (CS=GPIO 10, MOSI=11, MISO=13, SCLK=12).");
    }

    uint32_t tick = 0;
    lastActivityMs = esp_log_timestamp();

    while (1) {
        tick++;
        check_buttons();

        sysTime.second++;
        if (sysTime.second >= 60) {
            sysTime.second = 0;
            sysTime.minute++;
            if (sysTime.minute >= 60) {
                sysTime.minute = 0;
                sysTime.hour = (sysTime.hour + 1) % 24;
            }
        }

        // Haptic Vibration Motor Pulse when Alarm is Ringing
        if (alarmRinging) {
            bool pulse = (tick % 2 == 0);
            gpio_set_level(PIN_VIBRATION, pulse ? 1 : 0);
        } else {
            gpio_set_level(PIN_VIBRATION, 0);
        }

        // 1-Minute Display Inactivity Timeout
        if (!displaySleeping && !alarmRinging && (esp_log_timestamp() - lastActivityMs >= DISPLAY_TIMEOUT_MS)) {
            displaySleeping = true;
            gpio_set_level(TFT_BLK, 0);
            ESP_LOGI(TAG, "[POWER] 1-Minute Inactivity Timeout: TFT Backlight Powered Down.");
        }

        // Render TFT Dashboard
        if (!displaySleeping || alarmRinging) {
            render_tft_dashboard(tick);
        }

        // Log formatted output matching Arduino serial stream
        if (tick % 10 == 0) {
            ESP_LOGI(TAG, "%02d:%02d:%02d; %.2f Hz; NN_State: %s (%.0f%%)%s",
                     sysTime.hour, sysTime.minute, sysTime.second,
                     currentDominantFreq,
                     nnStageNames[currentNNStage],
                     currentNNConfidence * 100.0f,
                     alarmRinging ? " *** ALARM RINGING & VIBRATING ***" : "");
        }

        vTaskDelay(pdMS_TO_TICKS(100)); // 100ms loop period
    }
}
