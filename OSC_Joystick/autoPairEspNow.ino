// Scan all channels for the controller (blocking, up to ~14 s)
void findController() {
  isPaired = false;

  for (int channel = 1; channel <= 13; channel++) {
    if (!isPaired) testChannel(channel, false);

  }  // for channel 1 to 13
  if (!isPaired) { if (DEBUG)  Serial << "No success" << endl;
    delay(1000);
  }
}

// Send a pairing request on one channel and wait up to 1 s for the reply.
// keepWifi = true: stay connected to WiFi (only valid for the WiFi channel)
void testChannel(int channel, bool keepWifi) {
    if (DEBUG) Serial << "Try Channel: " << channel << endl;
    esp_now_deinit();                                         // Deinit existing ESP-NOW to change channel
    if (!keepWifi) {
      WiFi.disconnect();                                      // Disconnect WiFi and change channel
      delay(100);
      esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    }
    homeChannel = channel;
    if (esp_now_init() != ESP_OK) {                             // Re-initialize ESP-NOW
      if (DEBUG) Serial << "Error re-initializing ESP-NOW" << endl;
      return;
    }
    esp_now_register_recv_cb(OnDataRecv);                        // Re-register callbacks
    esp_now_register_send_cb(OnDataSent);
    pairing_message message = {};                               // Create broadcast pairing message
    WiFi.macAddress(message.mac_addr);
    message.channel = homeChannel;
    message.isPairing = true;
    uint8_t broadcastAddress[6];                                // Create broadcast address (all ones)
    memset(broadcastAddress, 0xFF, sizeof(broadcastAddress));
    esp_now_peer_info_t peerInfo = {};                         // Add broadcast peer // initializes all fields to 0
    memcpy(peerInfo.peer_addr, broadcastAddress, 6);
    peerInfo.channel = 0;  // Automatic channel
    peerInfo.encrypt = false;
    if (esp_now_is_peer_exist(broadcastAddress)) esp_now_del_peer(broadcastAddress);  // Check whether the peer already exists
    if (esp_now_add_peer(&peerInfo) == ESP_OK) {
      esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &message, sizeof(message)); // Send pairing message
      if (result == ESP_OK) { if (DEBUG) Serial << "Pairing request sent" << endl; }
        else if (DEBUG) Serial << "Error sending pairing request: " << result << endl;
    } else if (DEBUG) Serial << "Error adding broadcast peer"  << endl;

    unsigned long start = millis();                                            // Wait for reply
    while (!isPaired && millis() - start < 1000) delay(10);

    if (isPaired) {                                                            // Stop if pairing succeeded
      Serial << "Pairing successful! Crawler WiFi: " << (crawlerHasWlan ? "yes" : "no") << endl;
      sendFailCount = 0;
      return;
    }
    if (esp_now_is_peer_exist(broadcastAddress)) esp_now_del_peer(broadcastAddress);   // Remove broadcast peer before the next attempt
}


// Callback for sent data (signature changed with ESP-IDF 5.5 / ESP32 core 3.3)
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
void OnDataSent(const wifi_tx_info_t *tx_info, esp_now_send_status_t status) {
#else
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
#endif
  if (status == ESP_NOW_SEND_SUCCESS) {
    sendFailCount = 0;
  } else if (isPaired && ++sendFailCount >= MAX_SEND_FAILS) {   // crawler gone or on another channel
    isPaired = false;                                            // loop() starts a new channel scan
  }
}

// Callback for received data (signature changed with ESP32 core 3.x)
#if ESP_ARDUINO_VERSION_MAJOR >= 3
void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
#else
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
#endif
  if (len == sizeof(pairing_message)) {
    pairing_message message;
    memcpy(&message, incomingData, sizeof(message));
    if (!message.isPairing && !isPaired) {                                           // If we received a pairing confirmation
      if (DEBUG) Serial << "Pairing confirmation received!"  << endl;
      memcpy(controllerMacAddress, message.mac_addr, sizeof(controllerMacAddress));  // Store controller data
      crawlerHasWlan = message.hasWlan;
      esp_now_deinit();                                                              // Remove all existing peers and restart ESP-NOW
      homeChannel = message.channel;                                                 // Switch to the controller's channel
      if (WiFi.status() != WL_CONNECTED) esp_wifi_set_channel(homeChannel, WIFI_SECOND_CHAN_NONE);
      Serial.print("Switching to channel: ");
      Serial.println(homeChannel);
      if (esp_now_init() != ESP_OK) {                                               // Re-initialize ESP-NOW
        if (DEBUG) Serial << "Error re-initializing ESP-NOW"  << endl;
        return;
      }
      esp_now_register_recv_cb(OnDataRecv);                                          // Re-register callbacks
      esp_now_register_send_cb(OnDataSent);
      esp_now_peer_info_t peerInfo = {};  // initializes all fields to 0           // Establish peer connection
      memcpy(peerInfo.peer_addr, message.mac_addr, 6);
      peerInfo.channel = 0;              // Automatic channel
      peerInfo.encrypt = false;
      if (esp_now_add_peer(&peerInfo) == ESP_OK) {
        Serial.println("Controller added to peer list");
        isPaired = true;
      }
      else if (DEBUG) Serial << "Error adding controller as peer"  << endl;
    }  // is pairing msg
  }    // is size of pairing msg
}
