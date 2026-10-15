typedef struct pairing_message {
  uint8_t mac_addr[6];
  uint8_t channel;
  bool isPairing;
  bool hasWlan;      // reply only: crawler is connected to WiFi
} pairing_message;
// Message types are told apart by their length, so the sizes must differ
static_assert(sizeof(pairing_message) != sizeof(joystick_message), "ESP-NOW message sizes must differ");

// Start ESP-NOW and register the callbacks (ESP8266 and ESP32 have different APIs)
bool initEspNow() {
#if defined ESP8266
  if (esp_now_init() != 0) return false;
  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
#elif defined ESP32
  if (esp_now_init() != ESP_OK) return false;
#endif
  esp_now_register_recv_cb(OnDataRecv);
  esp_now_register_send_cb(OnDataSent);
  return true;
}

// Add (or replace) a peer; returns true on success
bool addEspNowPeer(const uint8_t *mac) {
#if defined ESP8266
  if (esp_now_is_peer_exist((uint8_t *)mac)) esp_now_del_peer((uint8_t *)mac);
  return esp_now_add_peer((uint8_t *)mac, ESP_NOW_ROLE_COMBO, 0, NULL, 0) == 0;
#elif defined ESP32
  esp_now_peer_info_t peerInfo = {};                                  // initializes all fields to 0
  memcpy(peerInfo.peer_addr, mac, 6);
  peerInfo.channel = 0;                                               // Automatic channel
  peerInfo.encrypt = false;
  if (esp_now_is_peer_exist(mac)) esp_now_del_peer(mac);              // Check whether the peer already exists
  return esp_now_add_peer(&peerInfo) == ESP_OK;
#endif
}

// Handle received ESP-NOW data (common for ESP8266 and ESP32)
void handleEspNowData(const uint8_t *mac, const uint8_t *incomingData, int len) {
  if (len == sizeof(pairing_message)) {
    if (FIXESP) Serial << "ESP-NOW pairing msg received" << endl;
    pairing_message message;
    memcpy(&message, incomingData, sizeof(message));
//    if (message.isPairing && !isPaired) {                                 // is pairing request
    if (message.isPairing) {                                 // is pairing request
      memcpy(transmitterMacAddress, mac, sizeof(transmitterMacAddress));  // Store transmitter MAC address
      if (!addEspNowPeer(mac)) {
        if (DEBUG) Serial << "Error adding peer" << endl;
      } else {
        pairing_message response;                                         // Prepare reply with own data
        WiFi.macAddress(response.mac_addr);
        response.channel = homeChannel;
        response.isPairing = false;                                       // This is a confirmation
        response.hasWlan = (WiFi.status() == WL_CONNECTED);
        int result = esp_now_send((uint8_t *) mac, (uint8_t *) &response, sizeof(response)); // Send reply
        if (result != 0) {
          if (DEBUG) Serial << "Error sending pairing confirmation: " << result << endl;
        } else isPaired = true;
      }
    } // is pairing request
  } // pairing msg
  else {   // accepted without pairing, so the joystick keeps working after a crawler reboot
    if (len == sizeof(joystick_message)) {        // only store here, applied in loop()
      memcpy((void *) &receivedData, incomingData, sizeof(receivedData));
      lastEspnowCmd = millis();
      espnowNew = true;
    }
  }
}

// Callbacks for received / sent data (signatures differ between ESP8266, ESP32 core 2.x and 3.x)
#if defined ESP8266
void OnDataRecv(uint8_t *mac, uint8_t *incomingData, uint8_t len) {
  handleEspNowData(mac, incomingData, len);
}

void OnDataSent(uint8_t *mac_addr, uint8_t status) {
  if (status != 0) if (DEBUG) Serial << "Send fail" << endl;
}
#elif defined ESP32
#if ESP_ARDUINO_VERSION_MAJOR >= 3
void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
  handleEspNowData(info->src_addr, incomingData, len);
}
#else
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
  handleEspNowData(mac, incomingData, len);
}
#endif

// Send callback signature changed with ESP-IDF 5.5 (ESP32 core 3.3)
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
void OnDataSent(const wifi_tx_info_t *tx_info, esp_now_send_status_t status) {
#else
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
#endif
  if (status != ESP_NOW_SEND_SUCCESS) if (DEBUG) Serial << "Send fail" << endl;
}
#endif

// Apply ESP-NOW joystick data in loop() context (keeps I2C access in one task)
// button1 = fire (held), button2 = full front light (toggle state)
void handleEspnowControl() {
  static bool active = false;
  static bool fireOn = false;
  static int8_t lightOn = -1;   // unknown until the first packet
  if (espnowNew) {
    espnowNew = false;
    active = true;
    joystick_message data;
    memcpy(&data, (const void *) &receivedData, sizeof(data));
    processJoystickInput(data.x, data.y);
    if (crawlerType == CRAWLER_PCA) {
      if (data.button1 != fireOn) {
        fireOn = data.button1;
        setLed(fireOut, fireOn ? 4095 : 0);
      }
      if (data.button2 != lightOn) {
        lightOn = data.button2;
        setFrontLight(lightOn ? 65 : 0);
      }
    }
  } else if (active && millis() - lastEspnowCmd > ESPNOW_TIMEOUT) {  // joystick gone or switched to WiFi
    active = false;
    stopMotors();
    if (fireOn && crawlerType == CRAWLER_PCA) setLed(fireOut, 0);
    fireOn = false;
  }
}
