// 6 - lights
// 7 - front lights
// 8 - camera
// 9 - restructured
// 10 - ESP-Now Handshake
//#define ESP8266
#define ESP32
#define FPV
#define DEBUG true
#define FIXESP true
#if defined ESP8266
  #include <ESP8266WiFi.h>
  #include <espnow.h>
  #include <ESP8266mDNS.h>  // Include mDNS library for hostname resolution
  extern "C" {
  #include "user_interface.h" // Required for wifi_set_macaddr()
  #define SDA_PIN 4
  #define SCL_PIN 5
}
#elif defined ESP32
  #include <WiFi.h>
  #include "esp_wifi.h"
  #include <esp_now.h>
  #include <ESPmDNS.h>
  #define SDA_PIN 13
  #define SCL_PIN 14
#endif
#ifdef FPV
  #include "esp_camera.h"
  #include "esp_timer.h"
  #include "img_converters.h"
  #include "Arduino.h"
  #include "fb_gfx.h"
  #include "soc/soc.h"             // disable brownout problems
  #include "soc/rtc_cntl_reg.h"    // disable brownout problems
  #include "esp_http_server.h"
#endif
#include <Wire.h>
#include <WiFiUdp.h>
#include <OSCMessage.h>
#include <Adafruit_PWMServoDriver.h>
#include <ArduinoOTA.h>  // Include OTA library
#include <Streaming.h>

// Create PWM driver object
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();

// Crawler types
#define CRAWLER_PCA 1   // Crawler with PCA9685+DRV8833
#define CRAWLER_GPIO 2  // Crawler with direct GPIO to DRV8833

// Variable to store detected crawler type
uint8_t crawlerType = CRAWLER_PCA;
// cam definitions
#define PART_BOUNDARY "123456789000000000000987654321"

#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22
  
// Define motor control pins on PCA9685 (for PCA9685 crawler)
#define MOTOR1_PCA_PIN1 2  // PCA9685 pin for motor 1 input 1
#define MOTOR1_PCA_PIN2 3  // PCA9685 pin for motor 1 input 2
#define MOTOR2_PCA_PIN1 1  // PCA9685 pin for motor 2 input 1
#define MOTOR2_PCA_PIN2 0  // PCA9685 pin for motor 2 input 2
#define redRight   4 
#define whiteRight 5
#define frontRight 6
#define redLeft    8 
#define whiteLeft  9
#define frontLeft  10
#define stdBrightnessRed 200
#define stdBrightnessFrontLeft 3
#define stdBrightnessFrontRight 7
#define fireOut 12

#if defined ESP8266
  // Define direct GPIO pins for DRV8833 (for direct GPIO crawler)
  #define MOTOR1_GPIO_PIN1 12  //D6// GPIO pin for motor 1 input 1
  #define MOTOR1_GPIO_PIN2 14  //D5// GPIO pin for motor 1 input 2
  #define MOTOR2_GPIO_PIN1 5  //D1// GPIO pin for motor 2 input 1
  #define MOTOR2_GPIO_PIN2 4  //D2// GPIO pin for motor 2 input 2
  #define wifiLed 13 //D7
#elif defined ESP32
  #define MOTOR1_GPIO_PIN1 12   //GPIO pin for motor 1 input 1
  #define MOTOR1_GPIO_PIN2 13   //GPIO pin for motor 1 input 2
  #define MOTOR2_GPIO_PIN1 27   //GPIO pin for motor 2 input 1
  #define MOTOR2_GPIO_PIN2 25   //GPIO pin for motor 2 input 2
  #define wifiLed 14 
#endif

// PWM ESP32 GPIO Channels
#define MOTOR1_CHAN1 0
#define MOTOR1_CHAN2 1
#define MOTOR2_CHAN1 2
#define MOTOR2_CHAN2 3

// PWM parameters
#define PWM_FREQ 100      // PWM frequency in Hz
#define PWM_MAX 4095       // Maximum PWM value for PCA9685 (12-bit resolution)
#define GPIO_PWM_MAX 1023  // Maximum PWM value for ESP8266 GPIO (10-bit resolution)
#define GPIO_PWM_FREQ 20000
#define GPIO_PWM_RESOLUTION 10


// Motor control deadzone to prevent motor creep at low values
#define MOTOR_DEADZONE 100  // Values between -DEADZONE and DEADZONE will be treated as 0

// WiFi settings
// WiFi credentials
#include <Credentials.h>
const char* hostname = "ESP-Crawler";  // Hostname for OTA identification
IPAddress staticIP(192, 168, 0, 192);  // Static IP for the crawler
IPAddress gateway(192, 168, 0, 1);     // Gateway IP address
IPAddress subnet(255, 255, 255, 0);    // Subnet mask
uint8_t crawlerMAC[] = {0x3C, 0x61, 0x05, 0xD0, 0x53, 0x5C};

// OSC settings for TouchOSC
WiFiUDP udp;
const unsigned int localPort = 1234;  // Local port to listen for OSC messages
const unsigned int outPort = 9000;    // Port to send OSC messages (if needed)

// TouchOSC control variables
float xyPadX = 0.0;  // X value from XY pad (-1.0 to 1.0)
float xyPadY = 0.0;  // Y value from XY pad (-1.0 to 1.0)
bool toggleState = false;  // State of toggle button (true = on, false = off)
// global MotorSpeed
#define breakLightDuration 1000
#define webSpeed 150

// Structure to receive joystick data from ESP-NOW
typedef struct joystick_message {
  int16_t x;       // X-axis value (-512 to 512)
  int16_t y;       // Y-axis value (-512 to 512)
  bool button1;    // Optional button state
  bool button2;    // Optional button state
  uint8_t target;  // Target crawler (1 or 2, or 0 for both)
} joystick_message;

joystick_message receivedData;

// Transmitter MAC address (filled in during pairing)
uint8_t transmitterMacAddress[6] = {0};
bool isPaired = false;
uint8_t homeChannel;




boolean isPhone = false;
boolean fire = false;

#ifdef FPV
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

httpd_handle_t camera_httpd = NULL;
httpd_handle_t stream_httpd = NULL;
volatile bool streamActive = false;  // true while a client is watching the FPV stream; OTA is paused then
// Web control state: set by the HTTP handler, applied in loop() (keeps I2C access in one task)
#define WEB_TIMEOUT 500               // ms without web command -> stop motors and fire
volatile int16_t webX = 0;            // XY pad, -512..512 like the joystick
volatile int16_t webY = 0;
volatile bool webFire = false;
volatile bool webNew = false;         // new XY command waiting
volatile bool webActive = false;      // web control currently driving
volatile unsigned long lastWebCmd = 0;
volatile int8_t webLightReq = -1;     // -1 = no request, 0 = off, 1 = full light
#endif
 
void setup() {
#ifdef FPV
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
#endif
  Serial.begin(115200);
#ifdef FPV
  camSetup();
#endif
#if defined ESP32  // because of PCA conflict with SDA
  pinMode(MOTOR1_GPIO_PIN1,OUTPUT);
  digitalWrite(MOTOR1_GPIO_PIN1,HIGH);
  pinMode(4,OUTPUT);
#endif
  Wire.begin(SDA_PIN, SCL_PIN);
  crawlerType = detectCrawlerType();
  if (crawlerType == CRAWLER_PCA) initPcaCrawler(); 
  else if (crawlerType == CRAWLER_GPIO) initGpioCrawler(); 
  stopMotors();
  ledsOff();
  setupWifi();  
  if (!MDNS.begin(hostname)) {Serial << "Error setting up MDNS responder!" << endl;}
    else {Serial << "mDNS responder started: " << hostname << ".local" << endl;}
  setupOTA();
  udp.begin(localPort);
  if (esp_now_init() != ESP_OK) if (DEBUG) Serial << "Error initializing ESP-NOW" << endl;
  esp_now_register_recv_cb(OnDataRecv);
  esp_now_register_send_cb(OnDataSent);
#ifdef FPV
  startCameraServer();
#endif
  Serial << "Crawler ready to receive commands" << endl;
}

void loop() {
#ifdef FPV
  if (!streamActive) ArduinoOTA.handle();  // OTA only when nobody watches the stream (avoids stutter)
  handleWebControl();
#else
  ArduinoOTA.handle();
  delay(5);
#endif
  checkOsc();
}
