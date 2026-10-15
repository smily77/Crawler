// Works with combi2 and combi3
// 2 - ESP-NOW
// 10 - ESP-NOW handshake
// 11 - WiFi (OSC) has priority, ESP-NOW as fallback, manual switch with L+R, OTA
#include <esp_now.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <esp_wifi.h>
#include <ArduinoOTA.h>
#include <OSCMessage.h>
#include <Streaming.h>
#include <Adafruit_NeoPixel.h>

#define DEBUG true
#define FIXESP true

// Automatic mode selection. Disabled by default for stability: without it the
// joystick uses WiFi if it can connect, otherwise ESP-NOW, and only the user
// switches (touch L+R). With it, the joystick also asks the crawler at startup
// whether it has WiFi and falls back to ESP-NOW when its own WiFi is lost.
//#define AUTO_MODE_SWITCH

// WiFi credentials
#include <Credentials.h>
const char* hostname = "ESP-Joystick";  // Hostname for OTA identification

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
#define SWITCH_ACTIVE HIGH       // touch switches L/R deliver HIGH when touched
#define MODE_SWITCH_HOLD 700     // ms both touch switches must be held to switch the mode
#define MAX_SEND_FAILS 25        // ESP-NOW send failures in a row (~0.5 s) before re-pairing
// Mode colors (right pixel)
#define COLOR_WLAN      0x002000 // green: WiFi / OSC
#define COLOR_ESPNOW    0x000020 // blue: ESP-NOW paired
#define COLOR_SEARCHING 0x201000 // orange: ESP-NOW, searching the crawler
// Threshold for detecting a minimal change
const float threshold = 0.02; // approx. 2% change

// ***********ESP-NOW **************
// Structure of the pairing message (must match the crawler)
typedef struct pairing_message {
  uint8_t mac_addr[6];
  uint8_t channel;
  bool isPairing;
  bool hasWlan;      // reply only: crawler is connected to WiFi
} pairing_message;

// Controller MAC address (filled in during pairing)
uint8_t controllerMacAddress[6] = {0};
volatile bool isPaired = false;
volatile bool crawlerHasWlan = false;
volatile int sendFailCount = 0;
uint8_t homeChannel = 1; // Start channel
int scanChannel = 1;     // next channel for the non-blocking scan in loop()

typedef struct joystick_message {
  int16_t x;       // X-axis value (-512 to 512)
  int16_t y;       // Y-axis value (-512 to 512)
  bool button1;    // fire (joystick button held)
  bool button2;    // full front light (enable button toggle state)
  uint8_t target;  // Target crawler (1 or 2, or 0 for both)
} joystick_message;
// Message types are told apart by their length, so the sizes must differ
static_assert(sizeof(pairing_message) != sizeof(joystick_message), "ESP-NOW message sizes must differ");

joystick_message dataToSend;
//esp_now_peer_info_t peerInfo;

WiFiUDP Udp;
Adafruit_NeoPixel pixels(noOfPixels, pixelPin, NEO_GRB + NEO_KHZ800);

boolean enableButton = false;
float lastX = -1.0;
float lastY = -1.0;
int lastBtn = -1;
boolean wlanOk = false;
boolean otaStarted = false;
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
  Serial << "Touch switch L/R idle level: " << digitalRead(switchL) << "/" << digitalRead(switchR) << endl;
  wlanOk = initWlan();
#ifdef AUTO_MODE_SWITCH
  if (wlanOk) {
    testChannel(WiFi.channel(), true);         // ask the crawler on our WiFi channel
    if (isPaired && crawlerHasWlan) useWlan();
      else useEspnow();                        // crawler not reachable via WiFi
  } else useEspnow();
#else
  if (wlanOk) useWlan(); else useEspnow();
#endif
}

void loop() {
  checkModeSwitch();
  checkEnableButton();

  // Read joystick values (0-4095)
  int joyX = analogRead(joystickXPin);
  int joyY = analogRead(joystickYPin);
  int btnState = !digitalRead(joystickBtnPin); // inverted, pull-up
  if (udpMsg) {
    ArduinoOTA.handle();
    sendOSCJoyStick(joyX, joyY, btnState);
#ifdef AUTO_MODE_SWITCH
    checkWlanLost();
#endif
  }
  else if (!isPaired) {                         // ESP-NOW: search the crawler, one channel per loop
    showMode();
    testChannel(scanChannel, false);
    scanChannel = scanChannel % 13 + 1;
  }
  else {
    sendEspnowJoyStick(joyX, joyY, btnState);
  }
  showMode();
  delay(20); // Short pause to relieve the CPU
}

// Use WiFi / OSC. Falls back to ESP-NOW if WiFi cannot be connected.
void useWlan() {
  if (WiFi.status() != WL_CONNECTED) wlanOk = initWlan();
  if (!wlanOk) {
    Serial << "WiFi not available - staying on ESP-NOW" << endl;
    useEspnow();
    return;
  }
  Udp.begin(homePort);
  if (!otaStarted) {
    setupOTA();
    otaStarted = true;
  }
  udpMsg = true;
  lastX = -1.0;                                  // force sending the current position
  if (enableButton) sendEnableButton();          // sync light state with the crawler
  Serial << "Mode: WiFi (OSC)" << endl;
  showMode();
}

// Use ESP-NOW. Leaves WiFi so the channel scan can run.
void useEspnow() {
  if (udpMsg && wlanOk) sendOSCStop();           // OSC has no timeout on the crawler
  udpMsg = false;
  WiFi.disconnect();
  isPaired = false;
  scanChannel = 1;
  Serial << "Mode: ESP-NOW" << endl;
  showMode();
}

// Toggle WiFi <-> ESP-NOW when both touch switches are held
void checkModeSwitch() {
  static unsigned long bothSince = 0;
  static bool waitRelease = false;
  bool l = (digitalRead(switchL) == SWITCH_ACTIVE);
  bool r = (digitalRead(switchR) == SWITCH_ACTIVE);
  if (waitRelease) {                             // wait until both are released again
    if (!l && !r) waitRelease = false;
    return;
  }
  if (!(l && r)) {
    bothSince = 0;
    return;
  }
  if (bothSince == 0) {
    bothSince = millis();
    return;
  }
  if (millis() - bothSince >= MODE_SWITCH_HOLD) {
    waitRelease = true;
    bothSince = 0;
    if (udpMsg) useEspnow(); else useWlan();
  }
}

// Enable button toggles the full front light (OSC: toggle1, ESP-NOW: button2)
void checkEnableButton() {
  if (!digitalRead(enableBtnPin)) {
    enableButton = ! enableButton;
    digitalWrite(enableLedPin,enableButton);
    while(!digitalRead(enableBtnPin)) {
      delay(10);
    }
    if (udpMsg) sendEnableButton();
  }
}

#ifdef AUTO_MODE_SWITCH
// Switch to ESP-NOW if WiFi is lost for more than 3 s
void checkWlanLost() {
  static unsigned long lostSince = 0;
  if (WiFi.status() == WL_CONNECTED) {
    lostSince = 0;
    return;
  }
  if (lostSince == 0) lostSince = millis();
  else if (millis() - lostSince > 3000) {
    Serial << "WiFi lost - switching to ESP-NOW" << endl;
    wlanOk = false;
    lostSince = 0;
    useEspnow();
  }
}
#endif

// Right pixel shows the mode: green = WiFi, blue = ESP-NOW, orange = searching
void showMode() {
  static unsigned long lastColor = 1;
  unsigned long color = udpMsg ? COLOR_WLAN : (isPaired ? COLOR_ESPNOW : COLOR_SEARCHING);
  if (color != lastColor) {
    lastColor = color;
    setLed(rightPixel, color);
  }
}
