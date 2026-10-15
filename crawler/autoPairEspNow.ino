typedef struct pairing_message {
  uint8_t mac_addr[6];
  uint8_t channel;
  bool isPairing;
} pairing_message;

// Callback for received data
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
  if (FIXESP) Serial << "ESP-NOW msg received" << endl;
  if (len == sizeof(pairing_message)) {
    pairing_message message;
    memcpy(&message, incomingData, sizeof(message));
//    if (message.isPairing && !isPaired) {                                 // is pairing request
    if (message.isPairing) {                                 // is pairing request
      memcpy(transmitterMacAddress, mac, sizeof(transmitterMacAddress));  // Store transmitter MAC address
      esp_now_peer_info_t peerInfo = {};                                  // Store peer - initializes all fields to 0
      memcpy(peerInfo.peer_addr, mac, 6);
      peerInfo.channel = 0;                                               // Automatic channel
      peerInfo.encrypt = false;
      if (esp_now_is_peer_exist(mac)) esp_now_del_peer(mac);              // Check whether the peer already exists
      if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        if (DEBUG) Serial << "Error adding peer" << endl;
      } else {
        pairing_message response;                                         // Prepare reply with own data
        WiFi.macAddress(response.mac_addr);
        response.channel = homeChannel;
        response.isPairing = false;                                       // This is a confirmation    
        esp_err_t result = esp_now_send(mac, (uint8_t *) &response, sizeof(response)); // Send reply
        if (result != ESP_OK) {
          if (DEBUG) Serial << "Error sending pairing confirmation: " << result << endl;
        } else isPaired = true;
      }
    } // is pairing request
  } // pairing msg
  else if (isPaired) {
    if (len == sizeof(joystick_message)) {
      memcpy(&receivedData, incomingData, sizeof(receivedData));
      processJoystickInput(receivedData.x, receivedData.y);
    }
  }
}

// Callback for sent data
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  if (status != ESP_NOW_SEND_SUCCESS) if (DEBUG) Serial << "Send fail" << endl;
}
