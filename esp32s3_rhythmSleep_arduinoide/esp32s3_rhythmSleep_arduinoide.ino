/*
  ===================================================================
  ESP32-S3 Integrated System with Neural Network Smart Alarm (Arduino IDE)
  ===================================================================
  Project Name: esp32s3_rhythmSleep_arduinoide.ino
  ===================================================================
*/

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <FS.h>
#include <SD.h>
#include <RTClib.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_ST7789.h>
#include <math.h>

// --- Pin Definitions ---
#define SDA_PIN        8
#define SCL_PIN        9
#define ANALOG_EEG_PIN 1

// 4 Physical Buttons (Active LOW with internal pull-ups)
#define BTN_MENU_PIN   4
#define BTN_UP_PIN     5
#define BTN_DOWN_PIN   6
#define BTN_SELECT_PIN 7  // OK / SELECT Button

// --- Haptic Vibration Motor Pin ---
#define PIN_VIBRATION  21

// --- ST7789 2.8" TFT Display Pins ---
#define TFT_MOSI       11
#define TFT_SCLK       12
#define TFT_MISO       13
#define TFT_CS         38
#define TFT_DC         39
#define TFT_RST        40
#define TFT_BLK        48  // Backlight LED Control Pin

// --- SD Card Module Pin ---
#define SD_CS_PIN      10

// --- USB-C Audio Pins ---
#define USB_AUDIO_DP_PIN 20  // USB D+
#define USB_AUDIO_DN_PIN 19  // USB D-
#define TONE_HALF_PERIOD_US 5000  // 5000 us = 5 ms (100 Hz tone)

// --- OLED Configuration ---
#define OLED_WIDTH     128
#define OLED_HEIGHT    64
#define OLED_RESET     -1
#define OLED_I2C_ADDR  0x3C

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

// --- FFT & Signal Processing Constants ---
#define SAMPLES         512
#define SAMPLING_FREQ   256.0
#define MIN_FREQ        0.5
#define MAX_FREQ        100.0
#define HISTORY_LEN     10

// 1-Minute Backlight Timeout (60,000 milliseconds)
#define DISPLAY_TIMEOUT_MS  60000

// 2-Minute Smart Alarm Rolling History Buffer (120 seconds)
#define SMART_ALARM_WINDOW_SEC 120

const unsigned long samplingPeriodUs = round(1000000.0 / SAMPLING_FREQ);

// --- Neural Network Dimensions (16 -> 32 -> 16 -> 4 MLP) ---
#define NN_INPUT_SIZE    16
#define NN_HIDDEN1_SIZE  32
#define NN_HIDDEN2_SIZE  16
#define NN_OUTPUT_SIZE   4

// Pre-trained Neural Network Weights from RhythmSleep
const float DEFAULT_NN_WEIGHTS[1140] PROGMEM = {
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
    -0.081820f, -0.091021f, -0.104508f, -0.111834f, -0.118542f, -0.123224f, -0.134146f, -0.140182f,
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
float nnRAMWeights[1140];

// Display Drivers & Peripherals
RTC_PCF8563 rtc;
Adafruit_SSD1306 oledDisplay(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET);
Adafruit_ST7789  tftDisplay = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

bool rtcAvailable   = false;
bool oledAvailable  = false;
bool tftAvailable   = false;
bool sdAvailable    = false;  // SD Card status flag
bool usbAudioActive = false; // USB Audio Status Flag

// Display Sleep / Backlight Timeout Tracking
unsigned long lastActivityMs = 0;
bool displaySleeping = false;

// TFT Redraw Tracking to Prevent Flickering
uint8_t lastTFTMenu = 255;
uint8_t lastTFTSec  = 255;
int16_t lastTFTFreqBarW = -1;

// FFT Buffers
double vReal[SAMPLES];
double vImag[SAMPLES];
uint16_t sampleIndex = 0;
unsigned long lastSampleMicros = 0;
double currentDominantFreq = 0.0;

// Temporal Feature History Ring Buffer
float featureHistory[HISTORY_LEN][NN_INPUT_SIZE] = {0};
uint8_t historyIndex = 0;
uint8_t historyCount = 0;

// Neural Network Inferred State
enum SleepStage : uint8_t { STAGE_WAKE = 0, STAGE_LIGHT = 1, STAGE_DEEP = 2, STAGE_REM = 3 };
SleepStage currentNNStage = STAGE_WAKE;
float nnConfidences[NN_OUTPUT_SIZE] = {0.25f, 0.25f, 0.25f, 0.25f};
float currentNNConfidence = 0.25f;

// --- 2-Minute Smart Alarm Rolling History Buffer ---
uint8_t sleepStageHistory120[SMART_ALARM_WINDOW_SEC] = {0};
float   lightConfHistory120[SMART_ALARM_WINDOW_SEC]  = {0.0f};
uint16_t history120Index = 0;
uint16_t history120Count = 0;

bool alarmRinging = false;
bool alarmTriggeredToday = false;

// Menu State
uint8_t currentMenu = 0; // 0: Time, 1: EEG Freq & NN State, 2: Alarm Settings Page

// Alarm Configuration
struct AlarmConfig {
  uint8_t startHour = 7;
  uint8_t startMin  = 0;
  uint8_t endHour   = 7;
  uint8_t endMin    = 30;
  bool    enabled   = false;
} alarmCfg;

uint8_t alarmEditField = 0;

// Button Tracking
struct Button {
  uint8_t pin;
  bool currentState;     
  bool previousReading;  
  unsigned long lastDebounceTime;
};

Button btnMenu   = {BTN_MENU_PIN, HIGH, HIGH, 0};
Button btnUp     = {BTN_UP_PIN, HIGH, HIGH, 0};
Button btnDown   = {BTN_DOWN_PIN, HIGH, HIGH, 0};
Button BTNSelect = {BTN_SELECT_PIN, HIGH, HIGH, 0}; // OK / SELECT Button

const unsigned long DEBOUNCE_DELAY = 40;

// Strings
const char* daysOfWeek[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
const char* monthsOfYear[] = {"", "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
const char* nnStageNames[] = {"WAKE", "LIGHT", "DEEP", "REM"};

// ===================================================================
// Audio Driver for USB-C DAC Soundcards
// ===================================================================

void initUSBHeadphoneAudio() {
  pinMode(USB_AUDIO_DP_PIN, OUTPUT);
  pinMode(USB_AUDIO_DN_PIN, OUTPUT);
  digitalWrite(USB_AUDIO_DP_PIN, LOW);
  digitalWrite(USB_AUDIO_DN_PIN, HIGH);

  usbAudioActive = true;
  Serial.printf("[USB DAC SUCCESS] Audio streamer active on USB D+ (GPIO %d) & D- (GPIO %d).\n", USB_AUDIO_DP_PIN, USB_AUDIO_DN_PIN);
}

void updateUSBHeadphoneAudioStream() {
  static unsigned long lastToggleUs = 0;

  // Stream 100 Hz differential audio wave to USB-C DAC at 5ms half-period (100 Hz)
  if (micros() - lastToggleUs >= TONE_HALF_PERIOD_US) {
    lastToggleUs = micros();

    static bool dacState = false;
    dacState = !dacState;

    if (!usbAudioActive) return;
    digitalWrite(USB_AUDIO_DP_PIN, dacState ? HIGH : LOW);
    digitalWrite(USB_AUDIO_DN_PIN, dacState ? LOW : HIGH);
  }
}

// ===================================================================
// SD Card Detection Helper (Initializes SPI cleanly for SD Card)
// ===================================================================

void checkSDCardDetection() {
  pinMode(SD_CS_PIN, OUTPUT);
  digitalWrite(SD_CS_PIN, HIGH);
  pinMode(TFT_CS, OUTPUT);
  digitalWrite(TFT_CS, HIGH);

  delay(20);

  // Initialize ESP32-S3 SPI Bus Pins explicitly before display
  SPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, SD_CS_PIN);

  // Pulse 80 dummy clocks with CS High to force SD card into SPI Mode
  digitalWrite(SD_CS_PIN, HIGH);
  for (int i = 0; i < 10; i++) {
    SPI.transfer(0xFF);
  }

  // Attempt SD initialization (4 MHz first, fallback to 400kHz for weak wires)
  bool mounted = SD.begin(SD_CS_PIN, SPI, 4000000);
  if (!mounted) {
    delay(50);
    mounted = SD.begin(SD_CS_PIN, SPI, 400000); // 400kHz slow mode
  }

  if (mounted) {
    uint8_t cardType = SD.cardType();
    if (cardType != CARD_NONE) {
      sdAvailable = true;
      uint64_t cardSize = SD.cardSize() / (1024 * 1024);
      Serial.printf("[SD CARD SUCCESS] SD Card detected. Type: ");
      if (cardType == CARD_MMC) Serial.print("MMC");
      else if (cardType == CARD_SD) Serial.print("SDSC");
      else if (cardType == CARD_SDHC) Serial.print("SDHC");
      else Serial.print("UNKNOWN");
      Serial.printf(" | Capacity: %llu MB (Dormant Mode)\n", cardSize);
    } else {
      sdAvailable = false;
      Serial.println("[SD CARD NOTICE] SD Card module connected, but slot is EMPTY.");
    }
  } else {
    sdAvailable = false;
    Serial.println("[SD CARD NOTICE] SD Card mount failed or module NOT connected (CS=GPIO 10, MOSI=11, MISO=13, SCLK=12).");
  }
}

// ===================================================================
// Serial Command Time Parser ("dd/mm/yyyy-hh:mm:ss")
// ===================================================================

void checkSerialTimeCommand() {
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();

    // Format expected: "dd/mm/yyyy-hh:mm:ss" (Length = 19 chars)
    if (input.length() >= 19) {
      int day = 0, month = 0, year = 0, hour = 0, minute = 0, second = 0;
      if (sscanf(input.c_str(), "%d/%d/%d-%d:%d:%d", &day, &month, &year, &hour, &minute, &second) == 6) {
        if (day >= 1 && day <= 31 && month >= 1 && month <= 12 && year >= 2020 && 
            hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59 && second >= 0 && second <= 59) {
          
          if (rtcAvailable) {
            rtc.adjust(DateTime(year, month, day, hour, minute, second));
            Serial.printf("[RTC SUCCESS] Time updated to: %02d/%02d/%04d %02d:%02d:%02d\n", 
                          day, month, year, hour, minute, second);
          } else {
            Serial.println("[RTC ERROR] PCF8563 RTC hardware not available.");
          }
        } else {
          Serial.println("[RTC ERROR] Date/time numbers out of valid ranges.");
        }
      } else {
        Serial.println("[RTC ERROR] Invalid format! Send string like: 01/08/2026-21:53:00");
      }
    }
  }
}

// ===================================================================
// Helper & Classification Strings
// ===================================================================

const char* getEEGBand(double freq) {
  if (freq < 4.0) return "Delta (Deep)";
  if (freq < 8.0) return "Theta (Drowsy)";
  if (freq < 13.0) return "Alpha (Relaxed)";
  if (freq < 30.0) return "Beta (Active)";
  return "Gamma (High Cog)";
}

// Wake Up Display helper
void wakeUpDisplay() {
  lastActivityMs = millis();
  if (displaySleeping) {
    displaySleeping = false;
    digitalWrite(TFT_BLK, HIGH); // Restore backlight LED voltage
    Serial.println("[POWER] Display woken up by user interaction.");
    lastTFTMenu = 255; // Force screen redraw on wake-up
  }
}

// ===================================================================
// OLED Rendering Functions (Dual Display Support)
// ===================================================================

void renderMenuTime(const DateTime &now) {
  oledDisplay.setTextSize(1);
  oledDisplay.setCursor(0, 0);
  oledDisplay.print("[1/3] TIME & DATE");
  oledDisplay.drawFastHLine(0, 11, 128, SSD1306_WHITE);

  char timeBuffer[10];
  snprintf(timeBuffer, sizeof(timeBuffer), "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
  oledDisplay.setTextSize(2);
  oledDisplay.setCursor(16, 18);
  oledDisplay.print(timeBuffer);

  oledDisplay.drawFastHLine(0, 38, 128, SSD1306_WHITE);

  char dateBuffer[25];
  snprintf(dateBuffer, sizeof(dateBuffer), "%s, %02d %s %04d", 
           daysOfWeek[now.dayOfTheWeek()], now.day(), monthsOfYear[now.month()], now.year());
  
  oledDisplay.setTextSize(1);
  int16_t x1, y1; uint16_t w, h;
  oledDisplay.getTextBounds(dateBuffer, 0, 0, &x1, &y1, &w, &h);
  oledDisplay.setCursor((128 - w) / 2, 44);
  oledDisplay.print(dateBuffer);
}

void renderMenuEEG() {
  oledDisplay.setTextSize(1);
  oledDisplay.setCursor(0, 0);
  oledDisplay.print("[2/3] EEG & NEURAL AI");
  oledDisplay.drawFastHLine(0, 11, 128, SSD1306_WHITE);

  oledDisplay.setCursor(0, 15);
  oledDisplay.printf("Peak Freq: %.2f Hz", currentDominantFreq);

  oledDisplay.setCursor(0, 27);
  oledDisplay.printf("Band: %s", getEEGBand(currentDominantFreq));

  oledDisplay.drawFastHLine(0, 38, 128, SSD1306_WHITE);

  oledDisplay.setCursor(0, 42);
  oledDisplay.printf("NN State: %s (%.0f%%)", nnStageNames[currentNNStage], currentNNConfidence * 100.0f);

  uint8_t barW = map((long)(currentDominantFreq * 10), (long)(MIN_FREQ * 10), (long)(MAX_FREQ * 10), 0, 124);
  if (barW > 124) barW = 124;
  oledDisplay.drawRect(0, 54, 128, 6, SSD1306_WHITE);
  oledDisplay.fillRect(2, 56, barW, 2, SSD1306_WHITE);
}

void renderMenuAlarm() {
  oledDisplay.setTextSize(1);
  oledDisplay.setCursor(0, 0);
  oledDisplay.print("[3/3] SMART ALARM");
  oledDisplay.drawFastHLine(0, 11, 128, SSD1306_WHITE);

  oledDisplay.setCursor(0, 16);
  oledDisplay.print("Start: ");
  if (alarmEditField == 1) oledDisplay.print(">");
  oledDisplay.printf("%02d", alarmCfg.startHour);
  oledDisplay.print(":");
  if (alarmEditField == 2) oledDisplay.print(">");
  oledDisplay.printf("%02d", alarmCfg.startMin);

  oledDisplay.setCursor(0, 28);
  oledDisplay.print("End:   ");
  if (alarmEditField == 3) oledDisplay.print(">");
  oledDisplay.printf("%02d", alarmCfg.endHour);
  oledDisplay.print(":");
  if (alarmEditField == 4) oledDisplay.print(">");
  oledDisplay.printf("%02d", alarmCfg.endMin);

  oledDisplay.setCursor(0, 40);
  oledDisplay.print("State: ");
  if (alarmEditField == 5) oledDisplay.print(">");
  oledDisplay.print(alarmCfg.enabled ? "[ SMART ON ]" : "[ OFF ]");

  oledDisplay.setCursor(0, 52);
  if (alarmEditField > 0) {
    oledDisplay.print("UP/DN:(5m) SEL:Next");
  } else {
    oledDisplay.print("SEL:Edit UP/DN:Tgl");
  }
}

void renderOLEDAlarmRinging(const DateTime &now) {
  oledDisplay.setTextSize(1);
  oledDisplay.setCursor(15, 5);
  oledDisplay.print("*** WAKE UP! ***");
  oledDisplay.drawFastHLine(0, 18, 128, SSD1306_WHITE);

  char tBuf[10];
  snprintf(tBuf, sizeof(tBuf), "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
  oledDisplay.setTextSize(2);
  oledDisplay.setCursor(16, 25);
  oledDisplay.print(tBuf);

  oledDisplay.setTextSize(1);
  oledDisplay.setCursor(15, 48);
  oledDisplay.print("Press OK to stop");
}

// ===================================================================
// ST7789 2.8" TFT Display Rendering
// ===================================================================

void drawTFTTouchButtons() {
  tftDisplay.drawFastHLine(0, 188, 320, ST7789_DARKGRAY);

  // Button 1: MENU
  tftDisplay.fillRect(5, 192, 70, 42, ST7789_BLUE);
  tftDisplay.drawRect(5, 192, 70, 42, ST7789_WHITE);
  tftDisplay.setTextColor(ST7789_WHITE);
  tftDisplay.setTextSize(2);
  tftDisplay.setCursor(15, 204);
  tftDisplay.print("MENU");

  // Button 2: UP (+)
  tftDisplay.fillRect(82, 192, 70, 42, ST7789_DARKGRAY);
  tftDisplay.drawRect(82, 192, 70, 42, ST7789_WHITE);
  tftDisplay.setCursor(102, 204);
  tftDisplay.print("UP+");

  // Button 3: DOWN (-)
  tftDisplay.fillRect(160, 192, 70, 42, ST7789_DARKGRAY);
  tftDisplay.drawRect(160, 192, 70, 42, ST7789_WHITE);
  tftDisplay.setCursor(170, 204);
  tftDisplay.print("DN-");

  // Button 4: SELECT / OK
  tftDisplay.fillRect(238, 192, 77, 42, ST7789_BLUE);
  tftDisplay.drawRect(238, 192, 77, 42, ST7789_WHITE);
  tftDisplay.setCursor(244, 204);
  tftDisplay.print("SELECT");
}

void renderTFTTime(const DateTime &now) {
  if (lastTFTSec == 255) {
    tftDisplay.setTextColor(ST7789_CYAN);
    tftDisplay.setTextSize(2);
    tftDisplay.setCursor(10, 10);
    tftDisplay.print("RhythmSleep [1/3] TIME");
    tftDisplay.drawFastHLine(0, 35, 320, ST7789_DARKGRAY);
  }

  char timeBuf[12];
  snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
  tftDisplay.setTextColor(ST7789_WHITE, ST7789_BLACK);
  tftDisplay.setTextSize(4);
  tftDisplay.setCursor(60, 65);
  tftDisplay.print(timeBuf);

  if (now.second() != lastTFTSec) {
    lastTFTSec = now.second();

    tftDisplay.drawFastHLine(20, 120, 280, ST7789_DARKGRAY);

    char dateBuf[30];
    snprintf(dateBuf, sizeof(dateBuf), "%s, %02d %s %04d", 
             daysOfWeek[now.dayOfTheWeek()], now.day(), monthsOfYear[now.month()], now.year());
    tftDisplay.setTextColor(ST7789_YELLOW, ST7789_BLACK);
    tftDisplay.setTextSize(2);
    tftDisplay.setCursor(45, 140);
    tftDisplay.print(dateBuf);
  }
}

void renderTFTEEG() {
  if (lastTFTSec == 255) {
    tftDisplay.setTextColor(ST7789_CYAN);
    tftDisplay.setTextSize(2);
    tftDisplay.setCursor(10, 10);
    tftDisplay.print("RhythmSleep [2/3] EEG AI");
    tftDisplay.drawFastHLine(0, 35, 320, ST7789_DARKGRAY);
    lastTFTSec = 0;
  }

  tftDisplay.setTextColor(ST7789_WHITE, ST7789_BLACK);
  tftDisplay.setTextSize(2);
  tftDisplay.setCursor(10, 50);
  tftDisplay.printf("Peak Freq : %6.2f Hz  ", currentDominantFreq);

  tftDisplay.setTextColor(ST7789_YELLOW, ST7789_BLACK);
  tftDisplay.setCursor(10, 80);
  tftDisplay.printf("Band      : %-18s", getEEGBand(currentDominantFreq));

  tftDisplay.setTextColor(ST7789_GREEN, ST7789_BLACK);
  tftDisplay.setCursor(10, 110);
  tftDisplay.printf("NN State  : %s (%2.0f%%)   ", nnStageNames[currentNNStage], currentNNConfidence * 100.0f);

  int16_t barW = map((long)(currentDominantFreq * 10), (long)(MIN_FREQ * 10), (long)(MAX_FREQ * 10), 0, 300);
  if (barW > 300) barW = 300;
  
  if (barW != lastTFTFreqBarW) {
    tftDisplay.drawRect(10, 150, 300, 20, ST7789_WHITE);
    tftDisplay.fillRect(12, 152, barW, 16, ST7789_CYAN);
    if (barW < lastTFTFreqBarW) {
      tftDisplay.fillRect(12 + barW, 152, lastTFTFreqBarW - barW, 16, ST7789_BLACK);
    }
    lastTFTFreqBarW = barW;
  }
}

void renderTFTAlarm() {
  if (lastTFTSec == 255) {
    tftDisplay.setTextColor(ST7789_CYAN);
    tftDisplay.setTextSize(2);
    tftDisplay.setCursor(10, 10);
    tftDisplay.print("RhythmSleep [3/3] SMART ALARM");
    tftDisplay.drawFastHLine(0, 35, 320, ST7789_DARKGRAY);
    lastTFTSec = 0;
  }

  tftDisplay.setTextSize(2);
  
  tftDisplay.setTextColor(ST7789_WHITE, ST7789_BLACK);
  tftDisplay.setCursor(10, 50);
  tftDisplay.print("Start Time: ");
  tftDisplay.setTextColor((alarmEditField == 1) ? ST7789_YELLOW : ST7789_WHITE, ST7789_BLACK);
  tftDisplay.printf("%02d", alarmCfg.startHour);
  tftDisplay.setTextColor(ST7789_WHITE, ST7789_BLACK);
  tftDisplay.print(":");
  tftDisplay.setTextColor((alarmEditField == 2) ? ST7789_YELLOW : ST7789_WHITE, ST7789_BLACK);
  tftDisplay.printf("%02d", alarmCfg.startMin);

  tftDisplay.setTextColor(ST7789_WHITE, ST7789_BLACK);
  tftDisplay.setCursor(10, 85);
  tftDisplay.print("End Time  : ");
  tftDisplay.setTextColor((alarmEditField == 3) ? ST7789_YELLOW : ST7789_WHITE, ST7789_BLACK);
  tftDisplay.printf("%02d", alarmCfg.endHour);
  tftDisplay.setTextColor(ST7789_WHITE, ST7789_BLACK);
  tftDisplay.print(":");
  tftDisplay.setTextColor((alarmEditField == 4) ? ST7789_YELLOW : ST7789_WHITE, ST7789_BLACK);
  tftDisplay.printf("%02d", alarmCfg.endMin);

  tftDisplay.setTextColor(ST7789_WHITE, ST7789_BLACK);
  tftDisplay.setCursor(10, 120);
  tftDisplay.print("State     : ");
  tftDisplay.setTextColor((alarmEditField == 5) ? ST7789_YELLOW : ST7789_WHITE, ST7789_BLACK);
  tftDisplay.print(alarmCfg.enabled ? "[ SMART ON ] " : "[ DISABLED ] ");

  tftDisplay.setTextColor(ST7789_LIGHTGRAY, ST7789_BLACK);
  tftDisplay.setTextSize(1);
  tftDisplay.setCursor(10, 155);
  tftDisplay.print("Press OK (SELECT) button to turn OFF ringing alarm");
}

void renderTFTAlarmRinging(const DateTime &now) {
  static bool toggleColor = false;
  toggleColor = !toggleColor;

  tftDisplay.fillRect(0, 0, 320, 240, toggleColor ? ST7789_RED : ST7789_YELLOW);

  tftDisplay.setTextColor(toggleColor ? ST7789_WHITE : ST7789_BLACK);
  tftDisplay.setTextSize(3);
  tftDisplay.setCursor(20, 30);
  tftDisplay.print("SMART ALARM!");

  char timeBuf[12];
  snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
  tftDisplay.setTextSize(4);
  tftDisplay.setCursor(60, 90);
  tftDisplay.print(timeBuf);

  tftDisplay.setTextSize(2);
  tftDisplay.setCursor(25, 160);
  tftDisplay.print("VIBRATING MOTOR...");
  tftDisplay.setCursor(15, 195);
  tftDisplay.print("Press OK Button to Stop");
}

// ===================================================================
// Neural Network & Math Calculations
// ===================================================================

void relu(float* x, int n) {
  for (int i = 0; i < n; i++) {
    if (x[i] < 0.0f) x[i] = 0.0f;
  }
}

void softmax(float* x, int n) {
  float maxVal = x[0];
  for (int i = 1; i < n; i++) {
    if (x[i] > maxVal) maxVal = x[i];
  }
  float sumExp = 0.0f;
  for (int i = 0; i < n; i++) {
    x[i] = expf(x[i] - maxVal);
    sumExp += x[i];
  }
  for (int i = 0; i < n; i++) {
    x[i] /= sumExp;
  }
}

void matmulRAM(const float* input, const float* w, const float* b, float* output, int inSize, int outSize) {
  for (int j = 0; j < outSize; j++) {
    float sum = b[j];
    for (int i = 0; i < inSize; i++) {
      sum += input[i] * w[j * inSize + i];
    }
    output[j] = sum;
  }
}

void runNeuralNetworkInference(double *vR, double *vI, uint16_t samples) {
  double binWidth = SAMPLING_FREQ / (double)samples;

  float deltaP = 0, thetaP = 0, alphaP = 0, betaP = 0, gammaP = 0, eegBandTotal = 0;
  float highFreqEnergy = 0;

  for (uint16_t i = 1; i < samples / 2; i++) {
    float freq = i * binWidth;
    float magSq = (vR[i] * vR[i] + vI[i] * vI[i]);

    if (freq >= 0.5f && freq < 4.0f)        deltaP += magSq;
    else if (freq >= 4.0f && freq < 8.0f)   thetaP += magSq;
    else if (freq >= 8.0f && freq < 13.0f)  alphaP += magSq;
    else if (freq >= 13.0f && freq < 30.0f) betaP += magSq;
    else if (freq >= 30.0f && freq <= 45.0f) gammaP += magSq;
    else if (freq > 45.0f)                  highFreqEnergy += magSq;
  }

  eegBandTotal = deltaP + thetaP + alphaP + betaP + gammaP;
  if (eegBandTotal < 1e-6f) eegBandTotal = 1e-6f;

  float relDelta = deltaP / eegBandTotal;
  float relTheta = thetaP / eegBandTotal;
  float relAlpha = alphaP / eegBandTotal;
  float relBeta  = betaP  / eegBandTotal;
  float relGamma = gammaP / eegBandTotal;

  float muscleWakeFactor = highFreqEnergy / (eegBandTotal + highFreqEnergy + 1e-5f);

  float rawFeatures[NN_INPUT_SIZE];
  auto clamp01 = [](float v) -> float { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); };

  rawFeatures[0]  = clamp01(relDelta);
  rawFeatures[1]  = clamp01(relTheta);
  rawFeatures[2]  = clamp01(relAlpha);
  rawFeatures[3]  = clamp01(relBeta + (muscleWakeFactor * 0.5f));  
  rawFeatures[4]  = clamp01(relGamma + (muscleWakeFactor * 0.5f)); 

  rawFeatures[5]  = clamp01((deltaP / (thetaP + 1e-5f)) / 10.0f);
  rawFeatures[6]  = clamp01((thetaP / (alphaP + 1e-5f)) / 10.0f);
  rawFeatures[7]  = clamp01((thetaP / (betaP  + 1e-5f)) / 10.0f);
  rawFeatures[8]  = clamp01(((deltaP + thetaP) / (alphaP + betaP + 1e-5f)) / 10.0f);

  rawFeatures[9]  = clamp01((currentDominantFreq - 0.5f) / 44.5f);
  
  float entropy = -(relDelta*logf(relDelta+1e-5f) + relTheta*logf(relTheta+1e-5f) + 
                    relAlpha*logf(relAlpha+1e-5f) + relBeta*logf(relBeta+1e-5f) + 
                    relGamma*logf(relGamma+1e-5f));
  rawFeatures[10] = clamp01(entropy / 2.5f);

  rawFeatures[11] = clamp01(logf(eegBandTotal + highFreqEnergy + 1.0f) / logf(10001.0f));
  rawFeatures[12] = clamp01(relAlpha + relBeta + muscleWakeFactor);
  rawFeatures[13] = clamp01(relDelta + relTheta);
  rawFeatures[14] = clamp01(sqrtf((eegBandTotal + highFreqEnergy) / (samples / 2)) / 200.0f);
  rawFeatures[15] = clamp01(relBeta + relGamma + muscleWakeFactor);

  for (int k = 0; k < NN_INPUT_SIZE; k++) {
    featureHistory[historyIndex][k] = rawFeatures[k];
  }
  historyIndex = (historyIndex + 1) % HISTORY_LEN;
  if (historyCount < HISTORY_LEN) historyCount++;

  float smoothedFeatures[NN_INPUT_SIZE] = {0};
  for (int h = 0; h < historyCount; h++) {
    for (int k = 0; k < NN_INPUT_SIZE; k++) {
      smoothedFeatures[k] += featureHistory[h][k];
    }
  }
  for (int k = 0; k < NN_INPUT_SIZE; k++) {
    smoothedFeatures[k] /= (float)historyCount;
  }

  const float* w1 = &nnRAMWeights[0];               
  const float* b1 = &nnRAMWeights[512];             
  const float* w2 = &nnRAMWeights[544];             
  const float* b2 = &nnRAMWeights[1056];            
  const float* w3 = &nnRAMWeights[1072];            
  const float* b3 = &nnRAMWeights[1136];            

  float out1[NN_HIDDEN1_SIZE];
  float out2[NN_HIDDEN2_SIZE];

  matmulRAM(smoothedFeatures, w1, b1, out1, NN_INPUT_SIZE, NN_HIDDEN1_SIZE);
  relu(out1, NN_HIDDEN1_SIZE);

  matmulRAM(out1, w2, b2, out2, NN_HIDDEN1_SIZE, NN_HIDDEN2_SIZE);
  relu(out2, NN_HIDDEN2_SIZE);

  matmulRAM(out2, w3, b3, nnConfidences, NN_HIDDEN2_SIZE, NN_OUTPUT_SIZE);
  softmax(nnConfidences, NN_OUTPUT_SIZE);

  if (currentDominantFreq > 30.0f || muscleWakeFactor > 0.3f) {
    nnConfidences[STAGE_WAKE] += 3.0f;
    softmax(nnConfidences, NN_OUTPUT_SIZE);
  }

  int bestClass = 0;
  float maxConf = nnConfidences[0];
  for (int i = 1; i < NN_OUTPUT_SIZE; i++) {
    if (nnConfidences[i] > maxConf) {
      maxConf = nnConfidences[i];
      bestClass = i;
    }
  }

  currentNNStage = static_cast<SleepStage>(bestClass);
  currentNNConfidence = maxConf;
}

// ===================================================================
// Smart Alarm Evaluator (2-Minute Light Sleep Window & >50% Certainty)
// ===================================================================

void updateSmartAlarm(const DateTime &now) {
  static unsigned long lastSecTick = 0;
  if (millis() - lastSecTick < 1000) return;
  lastSecTick = millis();

  // Record 1-second sleep stage & light sleep certainty into 120-second rolling buffer
  float lightCertainty = (currentNNStage == STAGE_LIGHT || currentNNStage == STAGE_WAKE) ? currentNNConfidence : 0.0f;
  
  sleepStageHistory120[history120Index] = (uint8_t)currentNNStage;
  lightConfHistory120[history120Index]  = lightCertainty;
  
  history120Index = (history120Index + 1) % SMART_ALARM_WINDOW_SEC;
  if (history120Count < SMART_ALARM_WINDOW_SEC) history120Count++;

  if (!alarmCfg.enabled) return;

  uint16_t currentMin = now.hour() * 60 + now.minute();
  uint16_t startMin   = alarmCfg.startHour * 60 + alarmCfg.startMin;
  uint16_t endMin     = alarmCfg.endHour * 60 + alarmCfg.endMin;

  bool insideWindow = false;
  if (startMin <= endMin) {
    insideWindow = (currentMin >= startMin && currentMin <= endMin);
  } else {
    insideWindow = (currentMin >= startMin || currentMin <= endMin);
  }

  if (insideWindow && !alarmTriggeredToday && !alarmRinging) {
    if (history120Count >= SMART_ALARM_WINDOW_SEC) {
      uint16_t lightStageSecs = 0;
      float lightConfSum = 0.0f;

      for (int i = 0; i < SMART_ALARM_WINDOW_SEC; i++) {
        if (sleepStageHistory120[i] == STAGE_LIGHT || sleepStageHistory120[i] == STAGE_WAKE) {
          lightStageSecs++;
        }
        lightConfSum += lightConfHistory120[i];
      }

      float avgCertainty = lightConfSum / (float)SMART_ALARM_WINDOW_SEC;

      // Smart condition: At least 2 mins in light sleep/wake AND >50% average certainty
      if (lightStageSecs >= 100 && avgCertainty > 0.50f) {
        alarmRinging = true;
        alarmTriggeredToday = true;
        wakeUpDisplay();
        Serial.printf("[SMART ALARM] Triggered! 2-min Light Sleep Avg Certainty: %.1f%%\n", avgCertainty * 100.0f);
      }
    }

    // Hard fallback: Ring if time reaches absolute endMin
    if (currentMin == endMin && !alarmTriggeredToday) {
      alarmRinging = true;
      alarmTriggeredToday = true;
      wakeUpDisplay();
      Serial.println("[SMART ALARM] End of window reached. Triggering hard wake-up alarm!");
    }
  }

  if (!insideWindow) {
    alarmTriggeredToday = false;
  }
}

// ===================================================================
// Single-Shot Physical Button Handling (4 Tactile Buttons)
// ===================================================================

bool isButtonPressed(Button &btn) {
  bool reading = digitalRead(btn.pin);
  bool trigger = false;

  if (reading != btn.previousReading) {
    btn.lastDebounceTime = millis();
    btn.previousReading = reading;
  }

  if ((millis() - btn.lastDebounceTime) > DEBOUNCE_DELAY) {
    if (reading == LOW && btn.currentState == HIGH) {
      btn.currentState = LOW;
      trigger = true;
    } 
    else if (reading == HIGH && btn.currentState == LOW) {
      btn.currentState = HIGH;
    }
  }

  return trigger;
}

void handleButtonActions() {
  bool btn1 = isButtonPressed(btnMenu);
  bool btn2 = isButtonPressed(btnUp);
  bool btn3 = isButtonPressed(btnDown);
  bool btn4 = isButtonPressed(BTNSelect); // OK / SELECT Button

  // When Alarm is Ringing, ONLY the OK (SELECT) button turns it OFF
  if (alarmRinging) {
    if (btn4) {
      alarmRinging = false;
      digitalWrite(PIN_VIBRATION, LOW); // Stop vibration motor
      lastTFTMenu = 255;
      Serial.println("[ALARM] Smart Alarm turned OFF by OK (SELECT) button.");
    }
    return;
  }

  if (btn1 || btn2 || btn3 || btn4) {
    // If screen is sleeping, wake it up
    if (displaySleeping) {
      wakeUpDisplay();
      return;
    }
    lastActivityMs = millis();
  }

  if (btn1) {
    alarmEditField = 0;
    currentMenu = (currentMenu + 1) % 3;
  }

  if (currentMenu == 2) {
    if (btn4) {
      alarmEditField = (alarmEditField + 1) % 6;
    }

    if (btn2) {
      switch (alarmEditField) {
        case 1: alarmCfg.startHour = (alarmCfg.startHour + 1) % 24; break;
        case 2: alarmCfg.startMin  = (alarmCfg.startMin + 5) % 60; break;
        case 3: alarmCfg.endHour   = (alarmCfg.endHour + 1) % 24; break;
        case 4: alarmCfg.endMin    = (alarmCfg.endMin + 5) % 60; break;
        case 5: alarmCfg.enabled   = !alarmCfg.enabled; break;
        default: alarmCfg.enabled  = true; break;
      }
    }

    if (btn3) {
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

// ===================================================================
// Non-Blocking High-Precision Sampling & FFT
// ===================================================================

void updateFFT() {
  if (micros() - lastSampleMicros >= samplingPeriodUs) {
    lastSampleMicros = micros();

    uint16_t rawVal = analogRead(ANALOG_EEG_PIN);
    vReal[sampleIndex] = (double)rawVal;
    vImag[sampleIndex] = 0.0;
    sampleIndex++;

    if (sampleIndex >= SAMPLES) {
      double sum = 0;
      for (uint16_t i = 0; i < SAMPLES; i++) sum += vReal[i];
      double mean = sum / SAMPLES;
      for (uint16_t i = 0; i < SAMPLES; i++) vReal[i] -= mean;

      applyHannWindow(vReal, SAMPLES);
      computeFFT(vReal, vImag, SAMPLES);
      currentDominantFreq = findDominantFrequency(vReal, vImag, SAMPLES, SAMPLING_FREQ, MIN_FREQ, MAX_FREQ);

      runNeuralNetworkInference(vReal, vImag, SAMPLES);

      sampleIndex = 0;
    }
  }
}

void applyHannWindow(double *vData, uint16_t samples) {
  for (uint16_t i = 0; i < samples; i++) {
    vData[i] *= 0.5 * (1.0 - cos(2.0 * M_PI * i / (samples - 1)));
  }
}

void computeFFT(double *vR, double *vI, uint16_t samples) {
  uint16_t j = 0;
  for (uint16_t i = 0; i < samples - 1; i++) {
    if (i < j) {
      double tempR = vR[i]; vR[i] = vR[j]; vR[j] = tempR;
      double tempI = vI[i]; vI[i] = vI[j]; vI[j] = tempI;
    }
    uint16_t k = samples >> 1;
    while (k <= j) {
      j -= k;
      k >>= 1;
    }
    j += k;
  }

  for (uint16_t len = 2; len <= samples; len <<= 1) {
    double ang = -2.0 * M_PI / len;
    double wlenR = cos(ang);
    double wlenI = sin(ang);
    for (uint16_t i = 0; i < samples; i += len) {
      double wR = 1.0;
      double wI = 0.0;
      for (uint16_t j = 0; j < len / 2; j++) {
        uint16_t u = i + j;
        uint16_t v = i + j + len / 2;
        double vrR = vR[v] * wR - vI[v] * wI;
        double vrI = vR[v] * wI + vI[v] * wR;

        vR[v] = vR[u] - vrR;
        vI[v] = vI[u] - vrI;
        vR[u] = vR[u] + vrR;
        vI[u] = vI[u] + vrI;

        double nextWR = wR * wlenR - wI * wlenI;
        double nextWI = wR * wlenI + wI * wlenR;
        wR = nextWR;
        wI = nextWI;
      }
    }
  }
}

double findDominantFrequency(double *vR, double *vI, uint16_t samples, double samplingFreq, double minF, double maxF) {
  double binWidth = samplingFreq / (double)samples;
  
  uint16_t minBin = (uint16_t)ceil(minF / binWidth);
  uint16_t maxBin = (uint16_t)floor(maxF / binWidth);

  if (minBin < 1) minBin = 1;
  if (maxBin >= samples / 2) maxBin = (samples / 2) - 1;

  double maxMagnitude = 0.0;
  uint16_t peakBin = minBin;

  for (uint16_t i = minBin; i <= maxBin; i++) {
    double magnitude = sqrt(vR[i] * vR[i] + vI[i] * vI[i]);
    if (magnitude > maxMagnitude) {
      maxMagnitude = magnitude;
      peakBin = i;
    }
  }

  return (peakBin * binWidth);
}

// ===================================================================
// Arduino Setup & Loop
// ===================================================================

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000);

  Serial.println("\n--- ESP32-S3 System (ST7789 TFT + USB DAC Audio + SD Card + PCF8563) ---");
  Serial.println("Send time sync via Serial: dd/mm/yyyy-hh:mm:ss (e.g. 01/08/2026-21:53:00)");

  // Copy PROGMEM NN weights to RAM
  memcpy_P(nnRAMWeights, DEFAULT_NN_WEIGHTS, sizeof(DEFAULT_NN_WEIGHTS));

  // Configure Button Pins
  pinMode(BTN_MENU_PIN, INPUT_PULLUP);
  pinMode(BTN_UP_PIN, INPUT_PULLUP);
  pinMode(BTN_DOWN_PIN, INPUT_PULLUP);
  pinMode(BTN_SELECT_PIN, INPUT_PULLUP);

  // Configure Haptic Vibration Motor Pin (GPIO 21)
  pinMode(PIN_VIBRATION, OUTPUT);
  digitalWrite(PIN_VIBRATION, LOW);

  // Configure TFT Backlight PWM Pin (GPIO 48)
  pinMode(TFT_BLK, OUTPUT);
  digitalWrite(TFT_BLK, HIGH);

  // Configure High-Res ADC (12-bit)
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  // Initialize Audio Driver for USB-C DAC Soundcard
  initUSBHeadphoneAudio();

  // Initialize Shared I2C Bus
  Wire.begin(SDA_PIN, SCL_PIN);

  // 1. Initialize SD Card Module (Clean SPI Init)
  checkSDCardDetection();

  // 2. Initialize ST7789 2.8" TFT Display (SPI)
  tftDisplay.init(240, 320);
  tftDisplay.setRotation(1);  // Landscape mode (320x240)
  tftDisplay.fillScreen(ST7789_BLACK);
  tftAvailable = true;

  // Splash Screen
  tftDisplay.setTextColor(ST7789_CYAN);
  tftDisplay.setTextSize(2);
  tftDisplay.setCursor(20, 40);
  tftDisplay.println("ESP32-S3 RhythmSleep AI");
  tftDisplay.drawFastHLine(20, 70, 280, ST7789_WHITE);
  tftDisplay.setTextColor(ST7789_WHITE);
  tftDisplay.setTextSize(1);
  tftDisplay.setCursor(20, 90);
  tftDisplay.printf("SD Card: %s\n", sdAvailable ? "Detected (Dormant)" : "NOT FOUND / Mount Error");
  tftDisplay.setCursor(20, 105);
  tftDisplay.println("USB-C DAC: Audio Active");
  tftDisplay.setCursor(20, 120);
  tftDisplay.println("Serial Time Sync: dd/mm/yyyy-hh:mm:ss");

  // 3. Initialize OLED Display (I2C)
  if (oledDisplay.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR)) {
    oledAvailable = true;
    oledDisplay.clearDisplay();
    oledDisplay.setTextColor(SSD1306_WHITE);
    oledDisplay.setTextSize(1);
    oledDisplay.setCursor(5, 10);
    oledDisplay.println("RhythmSleep AI Model");
    oledDisplay.drawFastHLine(5, 24, 118, SSD1306_WHITE);
    oledDisplay.setCursor(5, 36);
    oledDisplay.println(sdAvailable ? "SD Card: Detected" : "SD Card: NOT FOUND");
    oledDisplay.setCursor(5, 48);
    oledDisplay.println("USB DAC Active");
    oledDisplay.display();
  }

  delay(1500);

  // 4. Initialize PCF8563 RTC
  if (rtc.begin()) {
    rtcAvailable = true;
    if (rtc.lostPower()) {
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
  }

  tftDisplay.fillScreen(ST7789_BLACK);
  drawTFTTouchButtons();
  
  lastActivityMs = millis();
  lastSampleMicros = micros();
}

void loop() {
  // 1. Process Serial Time Sync Command ("dd/mm/yyyy-hh:mm:ss")
  checkSerialTimeCommand();

  // 2. Process Physical Button Actions (4 Tactile Buttons)
  handleButtonActions();

  // 3. Stream Audio Signal to USB-C DAC Soundcard
  updateUSBHeadphoneAudioStream();

  // 4. Drive Rapid Haptic Vibration Motor when Alarm is Ringing
  if (alarmRinging) {
    // Rapid 100ms ON / 100ms OFF haptic pulse on GPIO 21
    bool pulse = (millis() / 100) % 2;
    digitalWrite(PIN_VIBRATION, pulse ? HIGH : LOW);
  } else {
    digitalWrite(PIN_VIBRATION, LOW);
  }

  // 5. Check 1-Minute Display Inactivity Timeout
  if (!displaySleeping && !alarmRinging && (millis() - lastActivityMs >= DISPLAY_TIMEOUT_MS)) {
    displaySleeping = true;
    digitalWrite(TFT_BLK, LOW); // Drop LED backlight pin voltage (Sleep)
    Serial.println("[POWER] 1-Minute Inactivity Timeout: TFT Backlight Powered Down.");
  }

  // 6. Continuous Non-Blocking EEG Sampling & Temporal FFT + NN Inference
  updateFFT();

  // 7. Read Time
  DateTime now = rtcAvailable ? rtc.now() : DateTime(2026, 8, 1, (millis()/3600000)%24, (millis()/60000)%60, (millis()/1000)%60);

  // 8. Evaluate Smart EEG Alarm
  updateSmartAlarm(now);

  // 9. Render active menu on ST7789 2.8" TFT Display
  static unsigned long lastTFTRenderMs = 0;
  if (tftAvailable) {
    if (alarmRinging) {
      if (millis() - lastTFTRenderMs >= 400) { // Flashing Alarm Screen
        lastTFTRenderMs = millis();
        renderTFTAlarmRinging(now);
      }
    } 
    else if (!displaySleeping && (millis() - lastTFTRenderMs >= 100 || currentMenu != lastTFTMenu)) {
      lastTFTRenderMs = millis();

      if (currentMenu != lastTFTMenu) {
        tftDisplay.fillRect(0, 0, 320, 185, ST7789_BLACK);
        drawTFTTouchButtons();
        lastTFTMenu = currentMenu;
        lastTFTSec = 255;
      }

      if (currentMenu == 0) {
        renderTFTTime(now);
      } else if (currentMenu == 1) {
        renderTFTEEG();
      } else if (currentMenu == 2) {
        renderTFTAlarm();
      }
    }
  }

  // 10. Render active menu on 0.96" OLED Display
  if (oledAvailable) {
    oledDisplay.clearDisplay();
    if (alarmRinging) {
      renderOLEDAlarmRinging(now);
    } else {
      if (currentMenu == 0) renderMenuTime(now);
      else if (currentMenu == 1) renderMenuEEG();
      else if (currentMenu == 2) renderMenuAlarm();
    }
    oledDisplay.display();
  }

  // 11. Serial Output Stream
  static unsigned long lastSerialPrint = 0;
  if (millis() - lastSerialPrint >= 1000) {
    lastSerialPrint = millis();
    Serial.printf("%02d:%02d:%02d; %.2f Hz; NN_State: %s (%.0f%%)%s\n", 
                  now.hour(), now.minute(), now.second(), 
                  currentDominantFreq, 
                  nnStageNames[currentNNStage], 
                  currentNNConfidence * 100.0f,
                  alarmRinging ? " *** ALARM RINGING & VIBRATING ***" : "");
  }
}
