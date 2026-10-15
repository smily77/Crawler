//OSC
void sendOSCJoyStick(int joyX, int joyY, int btnState) {

  // Scale to TouchOSC field (0 to 1)
  float oscX = mapFloat(joyX, 2*midY-maxY, maxY, 0.33, 0.66);
  float oscY = mapFloat(joyY, 2*midX-maxX, maxX, 1.0, 0.0);

  // Check for a change (including button)
  if (abs(oscX - lastX) > threshold || abs(oscY - lastY) > threshold || btnState != lastBtn || repetitive) {
  
    // Prepare OSC message
    OSCMessage oscMsg("/Crawler/xy1");
    oscMsg.add(oscX);
    oscMsg.add(oscY);
    oscMsg.add(float(btnState)); // optional button

    // Send OSC message
    Udp.beginPacket(remoteIP, remotePort);
    oscMsg.send(Udp);
    Udp.endPacket();
    oscMsg.empty();

    // Serial debug output
    Serial.printf("Sent: X: %.2f, Y: %.2f, Btn: %d\n", oscX, oscY, btnState);

    // Update last values
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

// Neutral position, sent before leaving WiFi mode (OSC has no timeout on the crawler)
void sendOSCStop() {
    OSCMessage oscMsg("/Crawler/xy1");
    oscMsg.add(0.5f);
    oscMsg.add(0.5f);
    oscMsg.add(0.0f);
    Udp.beginPacket(remoteIP, remotePort);
    oscMsg.send(Udp);
    Udp.endPacket();
    oscMsg.empty();
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
  dataToSend.button1 = btnState;       // fire while the joystick button is held
  dataToSend.button2 = enableButton;   // full front light
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
  joyX = joyX/3;                       // limit steering to 1/3 like OSC (0.33..0.66)
  joyY = - joyY;
  
  // Update data to send
  dataToSend.x = joyX;
  dataToSend.y = joyY;
  bool changed = !(lastSentButtonState == dataToSend.button1 &&  lastSentJoyX == dataToSend.x && lastSentJoyY == dataToSend.y);
  if (!changed && !repetitive) {
    if (false) Serial.println("nothing to send");
  }
  else {
    lastSentButtonState = dataToSend.button1;
    lastSentJoyX = dataToSend.x;
    lastSentJoyY = dataToSend.y;
    esp_err_t result = esp_now_send(controllerMacAddress, (uint8_t *)&dataToSend, sizeof(dataToSend));
    if (changed) {                     // print only changes (sent every 20 ms)
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
