// Works with combi2 and combi3
// 2 - ESP-NOW
// 10 - ESP-NOW handshake
#include <esp_now.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <esp_wifi.h>
#include <OSCMessage.h>
#include <Streaming.h>
#include <Adafruit_NeoPixel.h>

#define DEBUG true
#define FIXESP true

// WiFi credentials
#include <Credentials.h>

// IP and port of the receiver (TouchOSC compatible)
const IPAddress remoteIP(192, 168, 0, 192);
const unsigned int remotePort = 1234;
const unsigned int homePort = 8888;
uint8_t crawlerMAC[] = {0x3C, 0x61, 0x05, 0xD0, 0x53, 0x5C};

#define repetitive true

#define maxX 4095
#define midX 1947
#define maxY 4095
#define midY 1936
//Pins
#define joystickXPin 33
#define joystickYPin 32
#define joystickBtnPin 25  // Optional button pin, change as needed
#define enableBtnPin 36
#define switchR 34
#define switchL 35
#define enableLedPin 27
#define pixelPin 14
#define noOfPixels 2
#define leftPixel 1
#define rightPixel 0
#define intLed 5
// Threshold for detecting a minimal change
const float threshold = 0.02; // approx. 2% change

// ***********ESP-NOW **************
// Structure of the pairing message
typedef struct pairing_message {
  uint8_t mac_addr[6];
  uint8_t channel;
  bool isPairing;
} pairing_message;

// Controller MAC address (filled in during pairing)
uint8_t controllerMacAddress[6] = {0};
bool isPaired = false;
uint8_t homeChannel = 1; // Start channel

typedef struct joystick_message {
  int16_t x;       // X-axis value (-512 to 512)
  int16_t y;       // Y-axis value (-512 to 512)
  bool button1;    // Optional button state
  bool button2;    // Optional button state (not used)
  uint8_t target;  // Target crawler (1 or 2, or 0 for both)
} joystick_message;

joystick_message dataToSend;
//esp_now_peer_info_t peerInfo;

WiFiUDP Udp;
Adafruit_NeoPixel pixels(noOfPixels, pixelPin, NEO_GRB + NEO_KHZ800);

boolean enableButton = false;
float lastX = -1.0;
float lastY = -1.0;
int lastBtn = -1;
boolean wlanOk = false;
boolean espnowOk = false;
boolean udpMsg = true; // Message send by UDP or ESP-NOW if false

// Joystick variables
int joystickCenterX = 1935;  // Center value for ESP32 ADC (12-bit)
int joystickCenterY = 1947;  // Center value for ESP32 ADC (12-bit)
const int joystickDeadzone = 50; // 200 Deadzone to prevent drift near center
bool buttonState = false;
bool lastButtonState = false;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;  // Debounce time in ms
bool lastSentButtonState = false;

int lastSentJoyX = 0;

int lastSentJoyY = 0;

// Helper function for scaling
float mapFloat(long x, long in_min, long in_max, float out_min, float out_max) {
  return (float)(x - in_min) * (out_max - out_min) / (float)(in_max - in_min) + out_min;

}
void setup() {
  Serial.begin(115200);
  initPins();
  initPixel();
  setLed(rightPixel,0x200000);
  wlanOk = initWlan();
  if (wlanOk) {
    Udp.begin(homePort);
    setLed(rightPixel,0x002000);
  }
  espnowOk = initESPNOW();
    if (!wlanOk && espnowOk) udpMsg = false;
    if (udpMsg) setLed(rightPixel,0x002000);
      else setLed(rightPixel,0x000020);
}

void loop() {
  
  // Read joystick values (0-4095)
  int joyX = analogRead(joystickXPin);
  int joyY = analogRead(joystickYPin);
  int btnState = !digitalRead(joystickBtnPin); // inverted, pull-up
  if (udpMsg) {
    sendOSCJoyStick(joyX, joyY, btnState);
  
    if (!digitalRead(enableBtnPin)) {
      enableButton = ! enableButton;
      digitalWrite(enableLedPin,enableButton);
      while(!digitalRead(enableBtnPin)) {
        delay(10);
      }
      sendEnableButton();    
    }
  }
  if (!udpMsg) {
    sendEspnowJoyStick(joyX, joyY, btnState);
  }
  delay(20); // Short pause to relieve the CPU
}
