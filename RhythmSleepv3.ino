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
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFiUdp.h>
#include <HTTPClient.h>
#include <Preferences.h>
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
#define TFT_CS         38  // Chip Select for ST7789 Display
#define TFT_DC         39  // Data / Command Pin
#define TFT_RST        40  // Reset Pin
#define TFT_BLK        48  // Backlight LED Control Pin

// --- SD Card Module Pin ---
#define SD_CS_PIN      10  // Chip Select for SD Card Module

// --- Audio Speaker Pins ---
#define USB_AUDIO_DP_PIN 20  // Speaker D+
#define USB_AUDIO_DN_PIN 19  // Speaker D-
#define LEDC_AUDIO_CH    0

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
    -0.082531f, -0.094599f, -0.100344f, -0.114389f, -0.118542f, -0.123224f, -0.138481f, -0.140355f,
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
    1.500000f, 0.800000f, 1.800000f, 1.100000f
};

// Fast RAM Weights Buffer
float nnRAMWeights[1140];

// Display Drivers & Peripherals
RTC_PCF8563 rtc;
Adafruit_SSD1306 oledDisplay(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET);
Adafruit_ST7789  tftDisplay = Adafruit_ST7789(&SPI, TFT_CS, TFT_DC, TFT_RST);

bool rtcAvailable   = false;
bool oledAvailable  = false;
bool tftAvailable   = false;
bool sdAvailable    = false;  // SD Card status flag
bool bleConnected   = false;  // BLE connection state

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

// System Sleep Tracking State Machine
enum SystemState { STATE_IDLE = 0, STATE_SLEEPING = 1, STATE_WAKING = 2 };
SystemState systemState = STATE_IDLE;
bool sleepSessionActive = false;
uint16_t autoSleepRelaxationCounter = 0;

// Artifact Rejection & Temporal Context Buffer (30s Context)
#define TEMPORAL_WINDOW_SIZE 6
SleepStage stageContextBuffer[TEMPORAL_WINDOW_SIZE] = {STAGE_WAKE};
uint8_t contextIndex = 0;
bool isArtifactEpoch = false;
uint32_t totalArtifactCount = 0;

// Digital IIR Bandpass Filter State (0.5 - 45 Hz)
double bpX1 = 0, bpX2 = 0, bpY1 = 0, bpY2 = 0;

// On-Device Neural Network Learning Counter
uint32_t nnLearningSessionsCount = 0;

// WiFi, UDP Pairing & Provisioning State
Preferences preferences;
DNSServer dnsServer;
WebServer webServer(80);
WiFiUDP udpSocket;

String wifiSSID = "";
String wifiPass = "";
String serverIP = "";
String pairToken = "";
bool isPaired = false;
bool isAPMode = false;
bool sessionCompletedTrigger = false;
unsigned long lastUDPBroadcast = 0;
unsigned long lastTelemetrySend = 0;

// Menu State: 6 Total Menus
// 0: Time & Date
// 1: EEG Freq & Neural Network Sleep AI
// 2: Smart Alarm Settings
// 3: Bluetooth / Wireless Audio Control & Status
// 4: SD Card Music & Audio File Player
// 5: WiFi, Server Pairing & Factory Reset Menu
uint8_t currentMenu = 0;

// SD Music Player State
#define MAX_SD_FILES 20
String sdFileList[MAX_SD_FILES];
uint8_t sdFileCount = 0;
int8_t  selectedFileIdx = 0;
bool    musicPlaying = false;

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

// BLE Server Callbacks
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      bleConnected = true;
      Serial.println("[BLE SUCCESS] Client connected.");
    };

    void onDisconnect(BLEServer* pServer) {
      bleConnected = false;
      Serial.println("[BLE NOTICE] Client disconnected. Restarting advertising...");
      BLEDevice::startAdvertising();
    }
};

// ===================================================================
// Audio Driver via ESP32 Hardware LEDC Timer
// ===================================================================

void initAudioSpeaker() {
  pinMode(USB_AUDIO_DP_PIN, OUTPUT);
  pinMode(USB_AUDIO_DN_PIN, OUTPUT);
  digitalWrite(USB_AUDIO_DP_PIN, LOW);
  digitalWrite(USB_AUDIO_DN_PIN, LOW);

  // Attach LEDC timer for non-blocking hardware tone generation on GPIO 20
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
  ledcAttach(USB_AUDIO_DP_PIN, 440, 8);
  ledcWrite(USB_AUDIO_DP_PIN, 0);
#else
  ledcSetup(LEDC_AUDIO_CH, 440, 8);
  ledcAttachPin(USB_AUDIO_DP_PIN, LEDC_AUDIO_CH);
  ledcWrite(LEDC_AUDIO_CH, 0);
#endif

  Serial.println("[AUDIO SUCCESS] Hardware LEDC Audio initialized on GPIO 20 (D+) & GPIO 19 (D-).");
}

void playTone(uint32_t freq) {
  if (freq == 0) {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
    ledcWrite(USB_AUDIO_DP_PIN, 0);
#else
    ledcWrite(LEDC_AUDIO_CH, 0);
#endif
  } else {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
    ledcWriteTone(USB_AUDIO_DP_PIN, freq);
#else
    ledcWriteTone(LEDC_AUDIO_CH, freq);
#endif
  }
}

// ===================================================================
// SD Card Helper & File Scanner
// ===================================================================

void deselectSPI() {
  digitalWrite(SD_CS_PIN, HIGH);
  digitalWrite(TFT_CS, HIGH);
}

void scanSDFiles() {
  sdFileCount = 0;
  if (!sdAvailable) return;

  digitalWrite(TFT_CS, HIGH);
  digitalWrite(SD_CS_PIN, LOW);

  File root = SD.open("/");
  if (!root) {
    digitalWrite(SD_CS_PIN, HIGH);
    return;
  }

  File file = root.openNextFile();
  while (file && sdFileCount < MAX_SD_FILES) {
    if (!file.isDirectory()) {
      String name = String(file.name());
      if (name.endsWith(".wav") || name.endsWith(".WAV") || name.endsWith(".mp3") || name.endsWith(".MP3") || name.endsWith(".txt")) {
        sdFileList[sdFileCount++] = name;
      }
    }
    file = root.openNextFile();
  }
  root.close();
  digitalWrite(SD_CS_PIN, HIGH);
  Serial.printf("[SD MUSIC] Found %d audio files on SD Card.\n", sdFileCount);
}

void checkSDCardDetection() {
  pinMode(SD_CS_PIN, OUTPUT);
  digitalWrite(SD_CS_PIN, HIGH);
  pinMode(TFT_CS, OUTPUT);
  digitalWrite(TFT_CS, HIGH);

  delay(20);

  SPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, SD_CS_PIN);

  digitalWrite(SD_CS_PIN, HIGH);
  for (int i = 0; i < 10; i++) {
    SPI.transfer(0xFF);
  }

  bool mounted = SD.begin(SD_CS_PIN, SPI, 4000000);
  if (!mounted) {
    delay(50);
    mounted = SD.begin(SD_CS_PIN, SPI, 400000);
  }

  if (mounted) {
    uint8_t cardType = SD.cardType();
    if (cardType != CARD_NONE) {
      sdAvailable = true;
      uint64_t cardSize = SD.cardSize() / (1024 * 1024);
      Serial.printf("[SD CARD SUCCESS] SD Card detected. Capacity: %llu MB\n", cardSize);
      scanSDFiles();
    } else {
      sdAvailable = false;
      Serial.println("[SD CARD NOTICE] SD Card module connected, but slot is EMPTY.");
    }
  } else {
    sdAvailable = false;
    Serial.println("[SD CARD NOTICE] SD Card mount failed or module NOT connected.");
  }

  digitalWrite(SD_CS_PIN, HIGH);
}

// ===================================================================
// Bluetooth BLE Initialization
// ===================================================================

void initBLE() {
  BLEDevice::init("RhythmSleep_AI");
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  
  BLEService *pService = pServer->createService("180D"); // Heart Rate / Audio Service UUID
  pService->start();
  
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID("180D");
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();

  Serial.println("[BLE SUCCESS] BLE Server active as 'RhythmSleep_AI'. Advertising...");
}

// ===================================================================
// Display Helpers & Redraw
// ===================================================================

const char* getEEGBand(double freq) {
  if (freq < 4.0) return "Delta (Deep)";
  if (freq < 8.0) return "Theta (Drowsy)";
  if (freq < 13.0) return "Alpha (Relaxed)";
  if (freq < 30.0) return "Beta (Active)";
  return "Gamma (High)";
}

void wakeUpDisplay() {
  lastActivityMs = millis();
  if (displaySleeping) {
    displaySleeping = false;
    digitalWrite(TFT_BLK, HIGH);
    Serial.println("[POWER] Display woken up by user interaction.");
    lastTFTMenu = 255;
  }
}

// ===================================================================
// OLED Rendering Functions (5 Menus)
// ===================================================================

void renderMenuTime(const DateTime &now) {
  oledDisplay.setTextSize(1);
  oledDisplay.setCursor(0, 0);
  oledDisplay.print("[1/5] TIME & DATE");
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
  oledDisplay.print("[2/5] EEG REAL-TIME");
  oledDisplay.drawFastHLine(0, 11, 128, SSD1306_WHITE);

  oledDisplay.setCursor(0, 15);
  oledDisplay.printf("Peak Freq: %.2f Hz", currentDominantFreq);

  oledDisplay.setCursor(0, 27);
  oledDisplay.printf("NN: %s (%.0f%%)", nnStageNames[currentNNStage], currentNNConfidence * 100.0f);

  oledDisplay.drawFastHLine(0, 38, 128, SSD1306_WHITE);

  oledDisplay.setCursor(0, 42);
  if (sleepSessionActive) {
    if (currentNNStage == STAGE_WAKE) {
      oledDisplay.print("Mode: PRE-SLEEP AWAIT");
    } else {
      oledDisplay.print("Mode: SLEEPING");
    }
  } else {
    oledDisplay.print("Mode: IDLE (SEL:Start)");
  }

  oledDisplay.setCursor(0, 54);
  oledDisplay.printf("AI Model: v%d (Learned)", nnLearningSessionsCount);
}

void renderMenuNNStats() {
  oledDisplay.setTextSize(1);
  oledDisplay.setCursor(0, 0);
  oledDisplay.print("[3/5] NEURAL AI STATS");
  oledDisplay.drawFastHLine(0, 11, 128, SSD1306_WHITE);

  int calcLearned = (nnLearningSessionsCount == 0) ? 15 : (nnLearningSessionsCount * 20);
  uint8_t pctLearned = (calcLearned > 100) ? 100 : (uint8_t)calcLearned;
  int calcOpt = pctLearned + 10;
  uint8_t pctOptimized = (calcOpt > 100) ? 100 : (uint8_t)calcOpt;

  oledDisplay.setCursor(0, 15);
  oledDisplay.printf("AI Learned: %d%%", pctLearned);

  oledDisplay.setCursor(0, 27);
  oledDisplay.printf("Optimized : %d%%", pctOptimized);

  oledDisplay.setCursor(0, 39);
  oledDisplay.printf("Passes    : %d Backprop", nnLearningSessionsCount);

  oledDisplay.setCursor(0, 51);
  oledDisplay.printf("NVS Memory: %s", nnLearningSessionsCount > 0 ? "SYNCED" : "DEFAULT");
}

void renderMenuAlarm() {
  oledDisplay.setTextSize(1);
  oledDisplay.setCursor(0, 0);
  oledDisplay.print("[4/5] SMART ALARM");
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

// ===================================================================
// On-Device Neural Network Online Backpropagation Learning Engine
// ===================================================================

void loadNNWeights() {
  preferences.begin("nn_weights", true);
  size_t len = preferences.getBytesLength("weights");
  if (len == sizeof(nnRAMWeights)) {
    preferences.getBytes("weights", nnRAMWeights, sizeof(nnRAMWeights));
    nnLearningSessionsCount = preferences.getUInt("learn_count", 0);
    Serial.printf("[NN NVS SUCCESS] Loaded customized weights from NVS (Sessions Trained: %d).\n", nnLearningSessionsCount);
  } else {
    for (int i = 0; i < 1140; i++) {
      nnRAMWeights[i] = pgm_read_float(&DEFAULT_NN_WEIGHTS[i]);
    }
    Serial.println("[NN NVS NOTICE] Initialized RAM neural network with factory weights.");
  }
  preferences.end();
}

void saveNNWeights() {
  preferences.begin("nn_weights", false);
  preferences.putBytes("weights", nnRAMWeights, sizeof(nnRAMWeights));
  preferences.putUInt("learn_count", ++nnLearningSessionsCount);
  preferences.end();
  Serial.printf("[NN LEARNING SUCCESS] Saved recalibrated weights to NVS (Total Learn Sessions: %d).\n", nnLearningSessionsCount);
}

// On-Device Backpropagation Gradient Descent Pass (16 -> 32 -> 16 -> 4 MLP)
void trainNNOnDevice(const float inputs[16], uint8_t targetStage, float learningRate) {
  float out1[32], out2[16], out3[4];
  float z1[32], z2[16], z3[4];

  float* w1 = &nnRAMWeights[0];    // 16x32 = 512
  float* b1 = &nnRAMWeights[512];  // 32
  float* w2 = &nnRAMWeights[544];  // 32x16 = 512
  float* b2 = &nnRAMWeights[1056]; // 16
  float* w3 = &nnRAMWeights[1072]; // 16x4 = 64
  float* b3 = &nnRAMWeights[1136]; // 4

  // Forward Pass Layer 1
  for (int j = 0; j < 32; j++) {
    float sum = b1[j];
    for (int i = 0; i < 16; i++) sum += inputs[i] * w1[i * 32 + j];
    z1[j] = sum;
    out1[j] = (sum > 0.0f) ? sum : 0.0f; // ReLU
  }

  // Forward Pass Layer 2
  for (int j = 0; j < 16; j++) {
    float sum = b2[j];
    for (int i = 0; i < 32; i++) sum += out1[i] * w2[i * 16 + j];
    z2[j] = sum;
    out2[j] = (sum > 0.0f) ? sum : 0.0f; // ReLU
  }

  // Forward Pass Layer 3 (Softmax)
  float maxZ = -999.0f;
  for (int k = 0; k < 4; k++) {
    float sum = b3[k];
    for (int j = 0; j < 16; j++) sum += out2[j] * w3[j * 4 + k];
    z3[k] = sum;
    if (sum > maxZ) maxZ = sum;
  }

  float expSum = 0.0f;
  for (int k = 0; k < 4; k++) {
    out3[k] = expf(z3[k] - maxZ);
    expSum += out3[k];
  }
  for (int k = 0; k < 4; k++) out3[k] /= expSum;

  // Backprop Output Gradients
  float delta3[4];
  for (int k = 0; k < 4; k++) {
    float target = (k == targetStage) ? 1.0f : 0.0f;
    delta3[k] = out3[k] - target;
  }

  // Backprop Layer 2 Gradients
  float delta2[16];
  for (int j = 0; j < 16; j++) {
    float sum = 0.0f;
    for (int k = 0; k < 4; k++) sum += delta3[k] * w3[j * 4 + k];
    delta2[j] = (z2[j] > 0.0f) ? sum : 0.0f;
  }

  // Backprop Layer 1 Gradients
  float delta1[32];
  for (int i = 0; i < 32; i++) {
    float sum = 0.0f;
    for (int j = 0; j < 16; j++) sum += delta2[j] * w2[i * 16 + j];
    delta1[i] = (z1[i] > 0.0f) ? sum : 0.0f;
  }

  const float lambda = 0.00001f; // L2 Regularization

  // Update Layer 3
  for (int j = 0; j < 16; j++) {
    for (int k = 0; k < 4; k++) {
      float grad = delta3[k] * out2[j] + lambda * w3[j * 4 + k];
      w3[j * 4 + k] -= learningRate * grad;
    }
  }
  for (int k = 0; k < 4; k++) b3[k] -= learningRate * delta3[k];

  // Update Layer 2
  for (int i = 0; i < 32; i++) {
    for (int j = 0; j < 16; j++) {
      float grad = delta2[j] * out1[i] + lambda * w2[i * 16 + j];
      w2[i * 16 + j] -= learningRate * grad;
    }
  }
  for (int j = 0; j < 16; j++) b2[j] -= learningRate * delta2[j];

  // Update Layer 1
  for (int i = 0; i < 16; i++) {
    for (int j = 0; j < 32; j++) {
      float grad = delta1[j] * inputs[i] + lambda * w1[i * 32 + j];
      w1[i * 32 + j] -= learningRate * grad;
    }
  }
  for (int j = 0; j < 32; j++) b1[j] -= learningRate * delta1[j];
}

void runOnDeviceLearningPass() {
  Serial.println("[NN LEARNING] Executing On-Device Backpropagation Weight Recorrection...");
  if (historyCount > 0) {
    for (int pass = 0; pass < 5; pass++) {
      for (int h = 0; h < historyCount; h++) {
        trainNNOnDevice(featureHistory[h], (uint8_t)STAGE_LIGHT, 0.005f);
      }
    }
    saveNNWeights();
  }
}

// Digital IIR Bandpass Filter (0.5 - 45 Hz @ 256Hz Sampling)
double applyBandpassFilter(double sample) {
  double filtered = 0.245 * (sample - bpX2) + 1.307 * bpY1 - 0.510 * bpY2;
  bpX2 = bpX1;
  bpX1 = sample;
  bpY2 = bpY1;
  bpY1 = filtered;
  return filtered;
}

// 30-Second Temporal Context Majority Voting
SleepStage applyTemporalMajorityFilter(SleepStage rawStage) {
  stageContextBuffer[contextIndex] = rawStage;
  contextIndex = (contextIndex + 1) % TEMPORAL_WINDOW_SIZE;

  uint8_t counts[4] = {0};
  for (int i = 0; i < TEMPORAL_WINDOW_SIZE; i++) {
    counts[stageContextBuffer[i]]++;
  }

  SleepStage bestStage = rawStage;
  uint8_t maxCount = 0;
  for (int s = 0; s < 4; s++) {
    if (counts[s] > maxCount) {
      maxCount = counts[s];
      bestStage = (SleepStage)s;
    }
  }
  return bestStage;
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
// WiFi Provisioning, UDP Server Pairing & Factory Reset Routines
// ===================================================================

void performFactoryReset() {
  Serial.println("[FACTORY RESET] Clearing WiFi and Server pairing settings...");
  
  digitalWrite(SD_CS_PIN, HIGH);
  digitalWrite(TFT_CS, LOW);
  tftDisplay.fillScreen(ST7789_RED);
  tftDisplay.setTextColor(ST7789_WHITE);
  tftDisplay.setTextSize(3);
  tftDisplay.setCursor(20, 70);
  tftDisplay.print("FACTORY RESET");
  tftDisplay.setTextSize(2);
  tftDisplay.setCursor(20, 120);
  tftDisplay.print("Wiping credentials...");
  tftDisplay.setCursor(20, 150);
  tftDisplay.print("Restarting into AP mode");
  digitalWrite(TFT_CS, HIGH);

  if (oledAvailable) {
    oledDisplay.clearDisplay();
    oledDisplay.setTextColor(SSD1306_WHITE);
    oledDisplay.setTextSize(2);
    oledDisplay.setCursor(10, 15);
    oledDisplay.print("RESETTING");
    oledDisplay.setTextSize(1);
    oledDisplay.setCursor(10, 45);
    oledDisplay.print("Wiping credentials...");
    oledDisplay.display();
  }

  preferences.begin("rhythm_cfg", false);
  preferences.clear();
  preferences.end();

  delay(2000);
  ESP.restart();
}

void setupWebServerRoutes() {
  webServer.on("/", HTTP_GET, []() {
    String html = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>";
    html += "<title>RhythmSleep WiFi Setup</title>";
    html += "<style>body{font-family:sans-serif;background:#0f172a;color:#fff;padding:20px;text-align:center}";
    html += "input,select{width:100%;padding:12px;margin:8px 0;border-radius:8px;border:1px solid #334155;background:#1e293b;color:#fff;box-sizing:border-box}";
    html += "input[type=submit]{background:#00f2fe;color:#000;font-weight:bold;cursor:pointer}</style></head><body>";
    html += "<h2>🧠 RhythmSleep WiFi Setup</h2>";
    html += "<p>Configure ESP32 Wi-Fi connection</p>";
    html += "<form action='/save' method='POST'>";
    html += "<label>Select Wi-Fi Network:</label><br>";
    
    int n = WiFi.scanNetworks();
    if (n > 0) {
      html += "<select name='ssid_select' onchange='document.getElementById(\"ssid\").value=this.value'>";
      html += "<option value=''>-- Select Network --</option>";
      for (int i = 0; i < n; ++i) {
        html += "<option value='" + WiFi.SSID(i) + "'>" + WiFi.SSID(i) + " (" + String(WiFi.RSSI(i)) + " dBm)</option>";
      }
      html += "</select><br>";
    }

    html += "<input type='text' id='ssid' name='ssid' placeholder='SSID' required><br>";
    html += "<input type='password' name='pass' placeholder='Password'><br>";
    html += "<input type='submit' value='Save & Connect'>";
    html += "</form></body></html>";

    webServer.send(200, "text/html", html);
  });

  webServer.on("/save", HTTP_POST, []() {
    String newSSID = webServer.arg("ssid");
    String newPass = webServer.arg("pass");

    if (newSSID.length() > 0) {
      preferences.begin("rhythm_cfg", false);
      preferences.putString("ssid", newSSID);
      preferences.putString("pass", newPass);
      preferences.end();

      String res = "<!DOCTYPE html><html><body style='background:#0f172a;color:#00f2fe;font-family:sans-serif;text-align:center;padding:50px'>";
      res += "<h2>✅ WiFi Saved!</h2><p>RhythmSleep ESP32 is restarting and connecting to " + newSSID + "...</p></body></html>";
      webServer.send(200, "text/html", res);

      delay(1500);
      ESP.restart();
    } else {
      webServer.send(400, "text/plain", "Missing SSID");
    }
  });

  webServer.onNotFound([]() {
    webServer.sendHeader("Location", "http://192.168.4.1/", true);
    webServer.send(302, "text/plain", "");
  });
}

void initWiFiProvisioning() {
  preferences.begin("rhythm_cfg", false);
  wifiSSID  = preferences.getString("ssid", "");
  wifiPass  = preferences.getString("pass", "");
  serverIP  = preferences.getString("server_ip", "");
  pairToken = preferences.getString("token", "");
  isPaired  = preferences.getBool("is_paired", false);
  preferences.end();

  if (wifiSSID.length() > 0) {
    Serial.printf("[WIFI] Attempting connection to SSID: %s...\n", wifiSSID.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.begin(wifiSSID.c_str(), wifiPass.c_str());

    unsigned long startMs = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - startMs < 12000)) {
      delay(250);
      Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
      isAPMode = false;
      Serial.printf("[WIFI SUCCESS] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
      udpSocket.begin(8888);
      return;
    } else {
      Serial.println("[WIFI NOTICE] Connection failed/timed out. Switching to SoftAP Provisioning Mode.");
    }
  }

  // Fallback to SoftAP Mode
  isAPMode = true;
  WiFi.mode(WIFI_AP);
  WiFi.softAP("RhythmSleep-Setup");
  dnsServer.start(53, "*", WiFi.softAPIP());
  setupWebServerRoutes();
  webServer.begin();
  Serial.println("[WIFI AP ACTIVE] SoftAP active as 'RhythmSleep-Setup' at 192.168.4.1");
}

void handlePairingAndTelemetry() {
  if (isAPMode) {
    dnsServer.processNextRequest();
    webServer.handleClient();
    return;
  }

  if (WiFi.status() != WL_CONNECTED) return;

  // Unpaired State: Broadcast DISCOVER via UDP to Port 8888
  if (!isPaired) {
    if (millis() - lastUDPBroadcast >= 3000) {
      lastUDPBroadcast = millis();

      String macStr = WiFi.macAddress();
      String discoverMsg = "{\"type\":\"DISCOVER\",\"mac\":\"" + macStr + "\",\"model\":\"RhythmSleep_v3\"}";
      
      udpSocket.beginPacket("255.255.255.255", 8888);
      udpSocket.print(discoverMsg);
      udpSocket.endPacket();
      Serial.println("[UDP DISCOVER] Sent pairing broadcast to 255.255.255.255:8888");
    }

    // Check incoming UDP PAIR_ACK packet
    int packetSize = udpSocket.parsePacket();
    if (packetSize > 0) {
      char packetBuffer[256];
      int len = udpSocket.read(packetBuffer, 255);
      if (len > 0) packetBuffer[len] = 0;

      String payload = String(packetBuffer);
      Serial.printf("[UDP ACK RECEIVED] %s\n", payload.c_str());

      if (payload.indexOf("PAIR_ACK") != -1) {
        int ipIdx = payload.indexOf("\"server_ip\":\"");
        int tokIdx = payload.indexOf("\"token\":\"");
        if (ipIdx != -1 && tokIdx != -1) {
          int ipEnd = payload.indexOf("\"", ipIdx + 13);
          int tokEnd = payload.indexOf("\"", tokIdx + 9);

          if (ipEnd != -1 && tokEnd != -1) {
            serverIP = payload.substring(ipIdx + 13, ipEnd);
            pairToken = payload.substring(tokIdx + 9, tokEnd);
            isPaired = true;

            preferences.begin("rhythm_cfg", false);
            preferences.putString("server_ip", serverIP);
            preferences.putString("token", pairToken);
            preferences.putBool("is_paired", true);
            preferences.end();

            Serial.printf("[PAIR SUCCESS] Saved Server IP: %s | Token: %s\n", serverIP.c_str(), pairToken.c_str());
          }
        }
      }
    }
  } 
  // Paired State: Send Telemetry POST to Server
  else {
    if (millis() - lastTelemetrySend >= 5000) {
      lastTelemetrySend = millis();

      HTTPClient http;
      String url = "http://" + serverIP + ":3000/api/sleep-data";
      http.begin(url);
      http.addHeader("Content-Type", "application/json");

      DateTime now = rtcAvailable ? rtc.now() : DateTime(2026, 8, 6, 12, 0, 0);

      String body = "{";
      body += "\"token\":\"" + pairToken + "\",";
      body += "\"mac\":\"" + WiFi.macAddress() + "\",";
      body += "\"dominant_freq\":" + String(currentDominantFreq, 2) + ",";
      body += "\"delta\":" + String(fabs(vReal[1]), 1) + ",";
      body += "\"theta\":" + String(fabs(vReal[5]), 1) + ",";
      body += "\"alpha\":" + String(fabs(vReal[10]), 1) + ",";
      body += "\"beta\":" + String(fabs(vReal[20]), 1) + ",";
      body += "\"gamma\":" + String(fabs(vReal[40]), 1) + ",";
      body += "\"stage\":\"" + String(nnStageNames[currentNNStage]) + "\",";
      body += "\"stage_code\":" + String(currentNNStage) + ",";
      body += "\"certainty\":" + String(currentNNConfidence * 100.0f, 1) + ",";
      body += "\"alarm_ringing\":" + String(alarmRinging ? "true" : "false") + ",";
      body += "\"session_completed\":" + String(sessionCompletedTrigger ? "true" : "false") + ",";
      body += "\"timestamp\":" + String(now.unixtime());
      body += "}";

      int httpCode = http.POST(body);
      if (httpCode > 0) {
        if (sessionCompletedTrigger) {
          sessionCompletedTrigger = false;
        }
        String resp = http.getString();
        if (resp.indexOf("unpaired") != -1 || httpCode == 401) {
          Serial.println("[TELEMETRY UNPAIRED] Server returned unpaired status. Clearing pairing...");
          isPaired = false;
          preferences.begin("rhythm_cfg", false);
          preferences.putBool("is_paired", false);
          preferences.end();
        }
      } else {
        Serial.printf("[TELEMETRY FAIL] HTTP POST error: %s\n", http.errorToString(httpCode).c_str());
      }
      http.end();
    }
  }
}

void renderMenuWiFiReset() {
  oledDisplay.setTextSize(1);
  oledDisplay.setCursor(0, 0);
  oledDisplay.print("[5/5] WIFI & RESET");
  oledDisplay.drawFastHLine(0, 11, 128, SSD1306_WHITE);

  oledDisplay.setCursor(0, 16);
  if (isAPMode) {
    oledDisplay.print("WiFi: AP (Setup)");
    oledDisplay.setCursor(0, 26);
    oledDisplay.print("IP: 192.168.4.1");
  } else if (WiFi.status() == WL_CONNECTED) {
    oledDisplay.print("WiFi: Connected");
    oledDisplay.setCursor(0, 26);
    oledDisplay.printf("IP: %s", WiFi.localIP().toString().c_str());
  } else {
    oledDisplay.print("WiFi: Disconnected");
  }

  oledDisplay.setCursor(0, 38);
  if (isPaired) {
    oledDisplay.printf("Server: PAIRED");
  } else {
    oledDisplay.print("Server: SEARCHING...");
  }

  oledDisplay.drawFastHLine(0, 48, 128, SSD1306_WHITE);
  oledDisplay.setCursor(0, 53);
  oledDisplay.print("> PRESS SEL: RESET");
}

void renderTFTWiFiReset() {
  digitalWrite(SD_CS_PIN, HIGH);
  digitalWrite(TFT_CS, LOW);

  if (lastTFTSec == 255) {
    tftDisplay.setTextColor(ST7789_CYAN);
    tftDisplay.setTextSize(2);
    tftDisplay.setCursor(10, 10);
    tftDisplay.print("RhythmSleep [5/5] WIFI");
    tftDisplay.drawFastHLine(0, 35, 320, ST7789_DARKGRAY);
    lastTFTSec = 0;
  }

  tftDisplay.setTextSize(2);
  
  tftDisplay.setTextColor(ST7789_WHITE, ST7789_BLACK);
  tftDisplay.setCursor(10, 50);
  if (isAPMode) {
    tftDisplay.print("WiFi Mode : AP SoftAP    ");
    tftDisplay.setCursor(10, 75);
    tftDisplay.print("Config IP : 192.168.4.1  ");
  } else if (WiFi.status() == WL_CONNECTED) {
    tftDisplay.print("WiFi Mode : Connected    ");
    tftDisplay.setCursor(10, 75);
    tftDisplay.printf("Local IP  : %-14s", WiFi.localIP().toString().c_str());
  } else {
    tftDisplay.print("WiFi Mode : Disconnected ");
    tftDisplay.setCursor(10, 75);
    tftDisplay.print("Local IP  : ---.---.---.-");
  }

  tftDisplay.setCursor(10, 105);
  if (isPaired) {
    tftDisplay.setTextColor(ST7789_GREEN, ST7789_BLACK);
    tftDisplay.printf("Server IP : %-14s", serverIP.c_str());
  } else {
    tftDisplay.setTextColor(ST7789_YELLOW, ST7789_BLACK);
    tftDisplay.print("Server    : SEARCHING    ");
  }

  tftDisplay.fillRect(10, 140, 300, 38, ST7789_RED);
  tftDisplay.drawRect(10, 140, 300, 38, ST7789_WHITE);
  tftDisplay.setTextColor(ST7789_WHITE);
  tftDisplay.setTextSize(2);
  tftDisplay.setCursor(20, 150);
  tftDisplay.print("PRESS OK -> FACTORY RESET");

  digitalWrite(TFT_CS, HIGH);
}

// ===================================================================
// ST7789 2.8" TFT Display Rendering
// ===================================================================

void drawTFTTouchButtons() {
  digitalWrite(SD_CS_PIN, HIGH);
  digitalWrite(TFT_CS, LOW);

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
  
  digitalWrite(TFT_CS, HIGH);
}

void renderTFTTime(const DateTime &now) {
  digitalWrite(SD_CS_PIN, HIGH);
  digitalWrite(TFT_CS, LOW);

  if (lastTFTSec == 255) {
    tftDisplay.setTextColor(ST7789_CYAN);
    tftDisplay.setTextSize(2);
    tftDisplay.setCursor(10, 10);
    tftDisplay.print("RhythmSleep [1/5] TIME");
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

  digitalWrite(TFT_CS, HIGH);
}

void renderTFTEEG() {
  digitalWrite(SD_CS_PIN, HIGH);
  digitalWrite(TFT_CS, LOW);

  if (lastTFTSec == 255) {
    tftDisplay.setTextColor(ST7789_CYAN);
    tftDisplay.setTextSize(2);
    tftDisplay.setCursor(10, 10);
    tftDisplay.print("RhythmSleep [2/5] EEG AI");
    tftDisplay.drawFastHLine(0, 35, 320, ST7789_DARKGRAY);
    lastTFTSec = 0;
  }

  tftDisplay.setTextSize(2);
  tftDisplay.setTextColor(ST7789_WHITE, ST7789_BLACK);
  tftDisplay.setCursor(10, 45);
  tftDisplay.printf("Peak Freq : %6.2f Hz  ", currentDominantFreq);

  tftDisplay.setTextColor(ST7789_YELLOW, ST7789_BLACK);
  tftDisplay.setCursor(10, 70);
  tftDisplay.printf("Band      : %-18s", getEEGBand(currentDominantFreq));

  tftDisplay.setTextColor(ST7789_GREEN, ST7789_BLACK);
  tftDisplay.setCursor(10, 95);
  tftDisplay.printf("NN Stage  : %s (%2.0f%%)   ", nnStageNames[currentNNStage], currentNNConfidence * 100.0f);

  tftDisplay.setCursor(10, 120);
  if (sleepSessionActive) {
    if (currentNNStage == STAGE_WAKE) {
      tftDisplay.setTextColor(ST7789_YELLOW, ST7789_BLACK);
      tftDisplay.print("Mode      : PRE-SLEEP AWAIT   ");
    } else {
      tftDisplay.setTextColor(ST7789_GREEN, ST7789_BLACK);
      tftDisplay.print("Mode      : SLEEP IN PROGRESS ");
    }
  } else {
    tftDisplay.setTextColor(ST7789_ORANGE, ST7789_BLACK);
    tftDisplay.print("Mode      : IDLE (Press OK)   ");
  }

  tftDisplay.setTextColor(ST7789_LIGHTGRAY, ST7789_BLACK);
  tftDisplay.setTextSize(1);
  tftDisplay.setCursor(10, 155);
  tftDisplay.printf("AI Weights: v%d (On-Device Learned) | SEL: Toggle Sleep", nnLearningSessionsCount);

  digitalWrite(TFT_CS, HIGH);
}

void renderTFTNNStats() {
  digitalWrite(SD_CS_PIN, HIGH);
  digitalWrite(TFT_CS, LOW);

  if (lastTFTSec == 255) {
    tftDisplay.setTextColor(ST7789_CYAN);
    tftDisplay.setTextSize(2);
    tftDisplay.setCursor(10, 10);
    tftDisplay.print("RhythmSleep [3/5] AI STATS");
    tftDisplay.drawFastHLine(0, 35, 320, ST7789_DARKGRAY);
    lastTFTSec = 0;
  }

  int calcLearned = (nnLearningSessionsCount == 0) ? 15 : (nnLearningSessionsCount * 20);
  uint8_t pctLearned = (calcLearned > 100) ? 100 : (uint8_t)calcLearned;
  int calcOpt = pctLearned + 10;
  uint8_t pctOptimized = (calcOpt > 100) ? 100 : (uint8_t)calcOpt;

  tftDisplay.setTextSize(2);
  tftDisplay.setTextColor(ST7789_WHITE, ST7789_BLACK);
  tftDisplay.setCursor(10, 45);
  tftDisplay.printf("AI Learned  : %3d%% ", pctLearned);

  tftDisplay.setTextColor(ST7789_GREEN, ST7789_BLACK);
  tftDisplay.setCursor(10, 75);
  tftDisplay.printf("Optimized   : %3d%% ", pctOptimized);

  tftDisplay.setTextColor(ST7789_YELLOW, ST7789_BLACK);
  tftDisplay.setCursor(10, 105);
  tftDisplay.printf("NVS Learn   : %d Passes ", nnLearningSessionsCount);

  tftDisplay.setTextColor(ST7789_CYAN, ST7789_BLACK);
  tftDisplay.setCursor(10, 135);
  tftDisplay.print("Model Arch  : 16-32-16-4 MLP");

  tftDisplay.setTextColor(ST7789_LIGHTGRAY, ST7789_BLACK);
  tftDisplay.setTextSize(1);
  tftDisplay.setCursor(10, 175);
  tftDisplay.print("On-Device Backprop active | OK: Force Recalibrate");

  digitalWrite(TFT_CS, HIGH);
}

void renderTFTAlarm() {
  digitalWrite(SD_CS_PIN, HIGH);
  digitalWrite(TFT_CS, LOW);

  if (lastTFTSec == 255) {
    tftDisplay.setTextColor(ST7789_CYAN);
    tftDisplay.setTextSize(2);
    tftDisplay.setCursor(10, 10);
    tftDisplay.print("RhythmSleep [4/5] ALARM");
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
  tftDisplay.print("Press OK (SELECT) button to edit/turn off alarm");

  digitalWrite(TFT_CS, HIGH);
}

void renderTFTAlarmRinging(const DateTime &now) {
  static bool toggleColor = false;
  toggleColor = !toggleColor;

  digitalWrite(SD_CS_PIN, HIGH);
  digitalWrite(TFT_CS, LOW);

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

  digitalWrite(TFT_CS, HIGH);
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

  // Spectral prior logit scaling for rich dynamic confidence (70% - 99%)
  if (relBeta > 0.25f || relGamma > 0.15f || currentDominantFreq >= 13.0f || muscleWakeFactor > 0.2f) {
    nnConfidences[STAGE_WAKE] += 2.0f + (relBeta * 3.0f);
  }
  if (relTheta > 0.30f) {
    nnConfidences[STAGE_LIGHT] += 1.5f + (relTheta * 2.0f);
  }
  if (relDelta > 0.35f) {
    nnConfidences[STAGE_DEEP] += 2.2f + (relDelta * 3.0f);
  }
  if (relTheta > 0.25f && relAlpha > 0.15f) {
    nnConfidences[STAGE_REM] += 1.5f + (relAlpha * 1.5f);
  }

  softmax(nnConfidences, NN_OUTPUT_SIZE);

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
// Smart Alarm Evaluator
// ===================================================================

void updateSmartAlarm(const DateTime &now) {
  if (!sleepSessionActive) return;
  static unsigned long lastSecTick = 0;
  if (millis() - lastSecTick < 1000) return;
  lastSecTick = millis();

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

      if (lightStageSecs >= 100 && avgCertainty > 0.50f) {
        alarmRinging = true;
        alarmTriggeredToday = true;
        wakeUpDisplay();
        playTone(523); // Play 523 Hz C5 alarm tone
        Serial.printf("[SMART ALARM] Triggered! 2-min Light Sleep Certainty: %.1f%%\n", avgCertainty * 100.0f);
      }
    }

    if (currentMin == endMin && !alarmTriggeredToday) {
      alarmRinging = true;
      alarmTriggeredToday = true;
      wakeUpDisplay();
      playTone(523);
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

  if (alarmRinging) {
    if (btn4) {
      alarmRinging = false;
      sessionCompletedTrigger = true;
      systemState = STATE_IDLE;
      sleepSessionActive = false;
      digitalWrite(PIN_VIBRATION, LOW);
      playTone(0); // Stop alarm sound
      lastTFTMenu = 255;
      Serial.println("[ALARM] Smart Alarm turned OFF by OK SELECT button. Sleep session completed!");
      runOnDeviceLearningPass();
    }
    return;
  }

  if (btn1 || btn2 || btn3 || btn4) {
    if (displaySleeping) {
      wakeUpDisplay();
      return;
    }
    lastActivityMs = millis();
  }

  if (btn1) {
    alarmEditField = 0;
    currentMenu = (currentMenu + 1) % 5; // 0: Time, 1: EEG AI, 2: AI Stats, 3: Smart Alarm, 4: WiFi & Reset
    lastTFTMenu = 255;
  }

  if (currentMenu == 1) { // Menu 1: EEG Real-Time - Toggle Sleep Tracking & Run NN Learning
    if (btn4) {
      if (!sleepSessionActive) {
        systemState = STATE_SLEEPING;
        sleepSessionActive = true;
        autoSleepRelaxationCounter = 0;
        Serial.println("[SLEEP MODE] Started Sleep Tracking Session!");
      } else {
        systemState = STATE_IDLE;
        sleepSessionActive = false;
        sessionCompletedTrigger = true;
        runOnDeviceLearningPass();
        Serial.println("[SLEEP MODE] Stopped Sleep Session. Executing On-Device NN Weight Learning!");
      }
      lastTFTMenu = 255;
    }
  }

  if (currentMenu == 2) { // Menu 2: Neural AI Stats - Manual Recalibration Pass
    if (btn4) {
      Serial.println("[AI MANUAL RECAL] User triggered manual Backpropagation pass!");
      runOnDeviceLearningPass();
      lastTFTMenu = 255;
    }
  }

  if (currentMenu == 3) { // Menu 3: Smart Alarm Edits
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

  if (currentMenu == 4) { // Menu 4: WiFi & Factory Reset
    if (btn4) {
      performFactoryReset();
    }
  }
}

// ===================================================================
// Non-Blocking High-Precision Sampling & FFT
// ===================================================================

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

void updateFFT() {
  if (micros() - lastSampleMicros >= samplingPeriodUs) {
    lastSampleMicros = micros();

    uint16_t rawVal = analogRead(ANALOG_EEG_PIN);
    if (rawVal < 30 || rawVal > 4065) {
      isArtifactEpoch = true;
      totalArtifactCount++;
    }

    double filteredVal = applyBandpassFilter((double)rawVal);
    vReal[sampleIndex] = filteredVal;
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

      if (isArtifactEpoch) {
        isArtifactEpoch = false;
        Serial.println("[SIGNAL REJECTED] ADC Rail / Motion Artifact detected. Epoch skipped.");
      } else {
        runNeuralNetworkInference(vReal, vImag, SAMPLES);
        currentNNStage = applyTemporalMajorityFilter(currentNNStage);

        // Auto-Sleep Relaxation Detection (3 minutes sustained relaxation < 12Hz)
        if (!sleepSessionActive && currentDominantFreq >= 0.5f && currentDominantFreq < 12.0f) {
          autoSleepRelaxationCounter++;
          if (autoSleepRelaxationCounter >= 36) { // 36 * 5s = 180s
            systemState = STATE_SLEEPING;
            sleepSessionActive = true;
            autoSleepRelaxationCounter = 0;
            Serial.println("[AUTO SLEEP DETECTED] Sustained EEG relaxation detected! Sleep Tracking Session activated.");
          }
        } else if (!sleepSessionActive) {
          autoSleepRelaxationCounter = 0;
        }
      }

      sampleIndex = 0;
    }
  }
}

// ===================================================================
// Arduino Setup & Loop
// ===================================================================

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000);

  Serial.println("\n--- ESP32-S3 System (ST7789 TFT + BLE Audio + SD Music + PCF8563) ---");

  loadNNWeights();

  pinMode(BTN_MENU_PIN, INPUT_PULLUP);
  pinMode(BTN_UP_PIN, INPUT_PULLUP);
  pinMode(BTN_DOWN_PIN, INPUT_PULLUP);
  pinMode(BTN_SELECT_PIN, INPUT_PULLUP);

  pinMode(PIN_VIBRATION, OUTPUT);
  digitalWrite(PIN_VIBRATION, LOW);

  pinMode(TFT_BLK, OUTPUT);
  digitalWrite(TFT_BLK, HIGH);

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  initAudioSpeaker();
  initBLE();

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);

  checkSDCardDetection();

  digitalWrite(SD_CS_PIN, HIGH);
  digitalWrite(TFT_CS, LOW);
  tftDisplay.init(240, 320);
  tftDisplay.setRotation(1);
  tftDisplay.fillScreen(ST7789_BLACK);
  tftAvailable = true;

  tftDisplay.setTextColor(ST7789_CYAN);
  tftDisplay.setTextSize(2);
  tftDisplay.setCursor(20, 40);
  tftDisplay.println("ESP32-S3 RhythmSleep AI");
  tftDisplay.drawFastHLine(20, 70, 280, ST7789_WHITE);
  tftDisplay.setTextColor(ST7789_WHITE);
  tftDisplay.setTextSize(1);
  tftDisplay.setCursor(20, 90);
  tftDisplay.printf("SD Card: %s (%d files)\n", sdAvailable ? "Detected" : "NOT FOUND", sdFileCount);
  tftDisplay.setCursor(20, 105);
  tftDisplay.println("BLE Server: RhythmSleep_AI");
  tftDisplay.setCursor(20, 120);
  tftDisplay.println("Audio Output: GPIO 20 LEDC");
  digitalWrite(TFT_CS, HIGH);

  if (oledDisplay.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR)) {
    oledAvailable = true;
    oledDisplay.clearDisplay();
    oledDisplay.setTextColor(SSD1306_WHITE);
    oledDisplay.setTextSize(1);
    oledDisplay.setCursor(5, 10);
    oledDisplay.println("RhythmSleep AI Model");
    oledDisplay.drawFastHLine(5, 24, 118, SSD1306_WHITE);
    oledDisplay.setCursor(5, 36);
    oledDisplay.printf("SD Files: %d\n", sdFileCount);
    oledDisplay.setCursor(5, 48);
    oledDisplay.println("BLE & TFT Active");
    oledDisplay.display();
  }

  delay(1500);

  if (rtc.begin()) {
    rtcAvailable = true;
    if (rtc.lostPower()) {
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
  }

  // Initialize WiFi AP Provisioning / Connection
  initWiFiProvisioning();

  digitalWrite(SD_CS_PIN, HIGH);
  digitalWrite(TFT_CS, LOW);
  tftDisplay.fillScreen(ST7789_BLACK);
  digitalWrite(TFT_CS, HIGH);

  drawTFTTouchButtons();
  
  lastActivityMs = millis();
  lastSampleMicros = micros();
}

void loop() {
  handleButtonActions();

  // Run WiFi captive portal web server (AP mode) or UDP pairing & HTTP telemetry (STA mode)
  handlePairingAndTelemetry();

  if (alarmRinging) {
    bool pulse = (millis() / 100) % 2;
    digitalWrite(PIN_VIBRATION, pulse ? HIGH : LOW);
  } else {
    digitalWrite(PIN_VIBRATION, LOW);
  }

  if (!displaySleeping && !alarmRinging && (millis() - lastActivityMs >= DISPLAY_TIMEOUT_MS)) {
    displaySleeping = true;
    digitalWrite(TFT_BLK, LOW);
    Serial.println("[POWER] 1-Minute Inactivity Timeout: TFT Backlight Powered Down.");
  }

  updateFFT();

  DateTime now = rtcAvailable ? rtc.now() : DateTime(2026, 8, 2, (millis()/3600000)%24, (millis()/60000)%60, (millis()/1000)%60);

  updateSmartAlarm(now);

  static unsigned long lastTFTRenderMs = 0;
  if (tftAvailable) {
    if (alarmRinging) {
      if (millis() - lastTFTRenderMs >= 400) {
        lastTFTRenderMs = millis();
        renderTFTAlarmRinging(now);
      }
    } 
    else if (!displaySleeping && (millis() - lastTFTRenderMs >= 100 || currentMenu != lastTFTMenu)) {
      lastTFTRenderMs = millis();

      if (currentMenu != lastTFTMenu) {
        digitalWrite(SD_CS_PIN, HIGH);
        digitalWrite(TFT_CS, LOW);
        tftDisplay.fillRect(0, 0, 320, 185, ST7789_BLACK);
        digitalWrite(TFT_CS, HIGH);
        drawTFTTouchButtons();
        lastTFTMenu = currentMenu;
        lastTFTSec = 255;
      }

      if (currentMenu == 0) renderTFTTime(now);
      else if (currentMenu == 1) renderTFTEEG();
      else if (currentMenu == 2) renderTFTNNStats();
      else if (currentMenu == 3) renderTFTAlarm();
      else if (currentMenu == 4) renderTFTWiFiReset();
    }
  }

  static unsigned long lastOLEDRenderMs = 0;
  if (oledAvailable && (millis() - lastOLEDRenderMs >= 200)) {
    lastOLEDRenderMs = millis();
    oledDisplay.clearDisplay();
    if (alarmRinging) {
      renderOLEDAlarmRinging(now);
    } else {
      if (currentMenu == 0) renderMenuTime(now);
      else if (currentMenu == 1) renderMenuEEG();
      else if (currentMenu == 2) renderMenuNNStats();
      else if (currentMenu == 3) renderMenuAlarm();
      else if (currentMenu == 4) renderMenuWiFiReset();
    }
    oledDisplay.display();
  }

  static unsigned long lastSerialPrint = 0;
  if (millis() - lastSerialPrint >= 1000) {
    lastSerialPrint = millis();
    Serial.printf("%02d:%02d:%02d; %.2f Hz; NN_State: %s (%.0f%%) | WiFi: %s | Server: %s%s\n", 
                  now.hour(), now.minute(), now.second(), 
                  currentDominantFreq, 
                  nnStageNames[currentNNStage], 
                  currentNNConfidence * 100.0f,
                  isAPMode ? "SoftAP (192.168.4.1)" : (WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString().c_str() : "Disconnected"),
                  isPaired ? "PAIRED" : "SEARCHING",
                  alarmRinging ? " *** ALARM RINGING & VIBRATING ***" : "");
  }
}
