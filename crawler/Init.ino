
void setupWifi() {
   WiFi.mode(WIFI_STA);  // ESP-NOW + WiFi
#if defined ESP8266
  wifi_set_macaddr(STATION_IF, crawlerMAC);
#elif defined ESP32
  esp_err_t err = esp_wifi_set_mac(WIFI_IF_STA, &crawlerMAC[0]);
#endif
  WiFi.disconnect();
  WiFi.config(staticIP, gateway, subnet);
  // Check whether the ESP accepted the address:
  Serial << "Current MAC address of the ESP: " << WiFi.macAddress() << endl;
  WiFi.begin(ssid, password);
  int connectionAttempts = 0;
  while (WiFi.status() != WL_CONNECTED && connectionAttempts < 20) {
    if (crawlerType == CRAWLER_PCA) {
      setLed(whiteLeft, (millis() % 1000) < 500 ? 4000 : 0);
      setLed(whiteRight, (millis() % 1000) < 500 ? 4000 : 0);
    } 
    else if (crawlerType == CRAWLER_GPIO) {
      digitalWrite(wifiLed, !digitalRead(wifiLed));
    }
    delay(500);
    Serial.print(".");
    connectionAttempts++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    homeChannel = WiFi.channel();
    Serial << "\nWiFi connected IP address: " << WiFi.localIP() << "  Channel:   " << WiFi.channel() << endl;
    if (crawlerType == CRAWLER_PCA) {
      setLed(redLeft,200);
      setLed(redRight,200);
      setLed(frontLeft,stdBrightnessFrontLeft);
      setLed(frontRight,stdBrightnessFrontRight);
      setLed(whiteLeft,0);
      setLed(whiteRight,0); 
    }
    else if (crawlerType == CRAWLER_GPIO) {
      digitalWrite(wifiLed,HIGH);
    }
  }
}

void initPcaCrawler() {
  pwm.begin();
  pwm.setPWMFreq(PWM_FREQ);
}

void initGpioCrawler() {
  #if defined ESP8266
    analogWriteFreq(GPIO_PWM_FREQ);  
    pinMode(MOTOR1_GPIO_PIN1, OUTPUT);
    pinMode(MOTOR1_GPIO_PIN2, OUTPUT);
    pinMode(MOTOR2_GPIO_PIN1, OUTPUT);
    pinMode(MOTOR2_GPIO_PIN2, OUTPUT);
  #elif defined ESP32
  #if ESP_ARDUINO_VERSION_MAJOR >= 3
    // Core 3.x: attach GPIO pins to fixed PWM channels (same channels as on core 2.x)
    ledcAttachChannel(MOTOR1_GPIO_PIN1, GPIO_PWM_FREQ, GPIO_PWM_RESOLUTION, MOTOR1_CHAN1);
    ledcAttachChannel(MOTOR1_GPIO_PIN2, GPIO_PWM_FREQ, GPIO_PWM_RESOLUTION, MOTOR1_CHAN2);
    ledcAttachChannel(MOTOR2_GPIO_PIN1, GPIO_PWM_FREQ, GPIO_PWM_RESOLUTION, MOTOR2_CHAN1);
    ledcAttachChannel(MOTOR2_GPIO_PIN2, GPIO_PWM_FREQ, GPIO_PWM_RESOLUTION, MOTOR2_CHAN2);
  #else
    // Configure PWM channels
    ledcSetup(MOTOR1_CHAN1, GPIO_PWM_FREQ, GPIO_PWM_RESOLUTION);
    ledcSetup(MOTOR1_CHAN2, GPIO_PWM_FREQ, GPIO_PWM_RESOLUTION);
    ledcSetup(MOTOR2_CHAN1, GPIO_PWM_FREQ, GPIO_PWM_RESOLUTION);
    ledcSetup(MOTOR2_CHAN2, GPIO_PWM_FREQ, GPIO_PWM_RESOLUTION);
    // Attach GPIO pins to PWM channels
    ledcAttachPin(MOTOR1_GPIO_PIN1, MOTOR1_CHAN1);
    ledcAttachPin(MOTOR1_GPIO_PIN2, MOTOR1_CHAN2);
    ledcAttachPin(MOTOR2_GPIO_PIN1, MOTOR2_CHAN1);
    ledcAttachPin(MOTOR2_GPIO_PIN2, MOTOR2_CHAN2);
  #endif
  #endif 
  pinMode(wifiLed, OUTPUT);
}


void setupOTA() {
  ArduinoOTA.setHostname(hostname);
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }
    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial.println("Start updating " + type);  
    // Stop motors during update
    stopMotors();
  }); 
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  }); 
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });  
  ArduinoOTA.begin();
  Serial.println("OTA ready");
}
