//OSC
void sendOSCJoyStick(int joyX, int joyY, int btnState) {

  // Skalierung auf TouchOSC-Feld (0 bis 1)
  float oscX = mapFloat(joyX, 2*midY-maxY, maxY, 0.33, 0.66);
  float oscY = mapFloat(joyY, 2*midX-maxX, maxX, 1.0, 0.0);

  // Prüfe, ob Änderung vorliegt (inkl. Button)
  if (abs(oscX - lastX) > threshold || abs(oscY - lastY) > threshold || btnState != lastBtn || repetitive) {
  
    // OSC Nachricht vorbereiten
    OSCMessage oscMsg("/Crawler/xy1");
    oscMsg.add(oscX);
    oscMsg.add(oscY);
    oscMsg.add(float(btnState)); // optionaler Button

    // OSC Nachricht senden
    Udp.beginPacket(remoteIP, remotePort);
    oscMsg.send(Udp);
    Udp.endPacket();
    oscMsg.empty();

    // Debug-Ausgabe seriell
    Serial.printf("Gesendet: X: %.2f, Y: %.2f, Btn: %d\n", oscX, oscY, btnState);

    // Aktualisiere letzte Werte
    lastX = oscX;
    lastY = oscY;
    lastBtn = btnState;    
  }
}

void sendEnableButton() {
    OSCMessage oscMsg1("/Crawler/toggle1");
    oscMsg1.add(float(enableButton));
    Udp.beginPacket(remoteIP, remotePort);
    oscMsg1.send(Udp);
    Udp.endPacket();
    oscMsg1.empty();
}
/*
// ESP-Now
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  if (status != ESP_NOW_SEND_SUCCESS) {
    Serial << "Error sending data" << endl;;
  }
}
*/

void sendEspnowJoyStick(int rawX, int rawY, int btnState) {
  dataToSend.button1 = !btnState;
  // Map joystick values from 0-4095 to -512 to 512
  // Apply center offset and deadzone
  int joyX = rawX - joystickCenterX;
  int joyY = rawY - joystickCenterY;

  // Apply deadzone
  if (abs(joyX) < joystickDeadzone) {
    joyX = 0;
  }
  
  if (abs(joyY) < joystickDeadzone) {
    joyY = 0;
  }
  
  // Constrain to -512 to 512 range
  joyX = constrain(map(joyX, -joystickCenterX, joystickCenterX, -512, 512), -512, 512);
  joyY = constrain(map(joyY, -joystickCenterY, joystickCenterY, -512, 512), -512, 512);
joyX = joyX/5;
  joyY = - joyY;
  
  // Update data to send
  dataToSend.x = joyX;
  dataToSend.y = joyY;
  if (lastSentButtonState == dataToSend.button1 &&  lastSentJoyX == dataToSend.x && lastSentJoyY == dataToSend.y && (!repetitive)) {
    if (false) Serial.println("nothing to send");
  }
  else {
    lastSentButtonState = dataToSend.button1;
    lastSentJoyX = dataToSend.x;
    lastSentJoyY = dataToSend.y;
    esp_err_t result = esp_now_send(crawlerMAC, (uint8_t *)&dataToSend, sizeof(dataToSend));
    if (true) {
      if (result != ESP_OK) Serial.println("Error sending the data");
      Serial.print("X: ");
      Serial.print(joyX);
      Serial.print(" Y: ");
      Serial.print(joyY);
      Serial.print(" Button: ");
      Serial.println(dataToSend.button1 ? "PRESSED" : "RELEASED");
    }
  }
}
