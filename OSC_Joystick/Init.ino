void initPins() { 
  pinMode(joystickXPin, INPUT);
  pinMode(joystickYPin, INPUT);
  pinMode(joystickBtnPin, INPUT_PULLUP);
  pinMode(enableBtnPin,INPUT_PULLUP);
  pinMode(switchL,INPUT);
  pinMode(switchR,INPUT);
  pinMode(enableLedPin,OUTPUT);
  digitalWrite(enableLedPin,LOW);
  pinMode(intLed,OUTPUT);
  digitalWrite(intLed,HIGH);
}

void initPixel() {
  pixels.begin(); 
  pixels.clear();
  pixels.show(); 
}

void setLed(int ledNr, unsigned long color) {
  pixels.setPixelColor(ledNr,color);
  pixels.show();
}

boolean initWlan() {
  setLed(leftPixel,0x000000);
  WiFi.mode(WIFI_AP_STA);
  WiFi.disconnect(true);
  WiFi.begin(ssid, password);
  int connectionAttempts = 0;
  while (WiFi.status() != WL_CONNECTED && connectionAttempts < 20) {
    setLed(leftPixel, (millis() % 1000) < 500 ? 0x200000 : 0x000000); // More even blinking
    delay(500);
    Serial.print(".");
    connectionAttempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial << "\nWiFi connected IP address: " << WiFi.localIP() << "  Channel:   " << WiFi.channel() << endl;
    setLed(leftPixel,0x000000);
    return true;
  } else {
    Serial << "\nWifi connection failed" << endl;
    setLed(leftPixel,0x200000);
    return false;
  }
}
/*
boolean initESPNOW() {
  boolean espnowOk = true;
  if (esp_now_init() != ESP_OK) {
    Serial << "Error initializing ESP-NOW" << endl;
    espnowOk = false;
  }
  esp_now_register_send_cb(OnDataSent);
  memcpy(peerInfo.peer_addr, crawlerMAC, 6);
  if (!wlanOk) peerInfo.channel = 1;  
  // peerInfo.channel = wifiChannel;
  peerInfo.encrypt = false;
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial << "Failed to add peer" << endl;
    espnowOk = false;
  } 
  // Initialize data structure
  dataToSend.x = 0;
  dataToSend.y = 0;
  dataToSend.button1 = false;
  dataToSend.button2 = false;
  dataToSend.target = 0;  // 0 means send to all crawlers
  if (espnowOk) setLed(leftPixel,0x000000);
  return espnowOk;
}
*/

boolean initESPNOW() {
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return false;
  } 
  // Register callbacks
  esp_now_register_recv_cb(OnDataRecv);
  esp_now_register_send_cb(OnDataSent);
  findController();
  return isPaired;
}

  
