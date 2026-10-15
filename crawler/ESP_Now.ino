/*

#if defined ESP8266
  void OnDataRecv(uint8_t *mac, uint8_t *incomingData, uint8_t len) {
    if (len == sizeof(joystick_message)) {
      memcpy(&receivedData, incomingData, sizeof(receivedData));
      processJoystickInput(receivedData.x, receivedData.y);
    }
  }
#elif defined ESP32
  void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {  
  //void OnDataRecv(const esp_now_recv_info *info, const uint8_t *incomingData, int len) {  // new SDK 
    if (len == sizeof(joystick_message)) {
      memcpy(&receivedData, incomingData, sizeof(receivedData));
      processJoystickInput(receivedData.x, receivedData.y);
    }
  }
#endif

*/
