// TouchOSC message handler for XY pad
void handleXYMessage(OSCMessage &msg) {
  xyPadX = msg.getFloat(0);  // X value (-1.0 to 1.0)
  xyPadY = msg.getFloat(1);  // Y value (-1.0 to 1.0)
  xyPadX = xyPadX*2 - 1;
  xyPadY = xyPadY*2 - 1;
  Serial << xyPadX << "   " << xyPadY; 
  // Convert TouchOSC range (-1.0 to 1.0) to joystick range (-512 to 512)
  int16_t x = (int16_t)(xyPadX * 512);
  int16_t y = (int16_t)(xyPadY * 512);    
  if (msg.size() == 2){
    isHandy = true;
    if (toggleState) processJoystickInput(x, y);
      else stopMotors();
  }
  if (msg.size() == 3) {
    fire = (msg.getFloat(2) > 0.5);
    Serial << "   Fire: " << fire << endl;
    if (fire) setLed(fireOut,4095); else setLed(fireOut,0);
    isHandy = false;
    processJoystickInput(x, y);         
  }
}

// TouchOSC message handler for toggle button
void handleToggleMessage(OSCMessage &msg) {
  if (msg.size() == 1) {
    toggleState = (msg.getFloat(0) > 0.5);  // 1.0 = on, 0.0 = off
    if (isHandy) {
      if (!toggleState) stopMotors();
    }
    else if (crawlerType == CRAWLER_PCA) setFrontLight( (!toggleState) ? 0 : 65);      
  }
}

void checkOsc() {
    // Check for incoming OSC messages from TouchOSC
  OSCMessage oscMsg;
  int packetSize = udp.parsePacket(); 
  if (packetSize > 0) {
    byte packetBuffer[packetSize];
    udp.read(packetBuffer, packetSize);    
    oscMsg.fill(packetBuffer, packetSize); 
    // Route the messages to the appropriate handlers
    if (oscMsg.fullMatch("/Crawler/xy1")) {
      // XY pad message
      oscMsg.dispatch("/Crawler/xy1", handleXYMessage);
      Serial.println("XY_MSG");
    }
    else if (oscMsg.fullMatch("/Crawler/toggle1")) {
      // Toggle button message
      oscMsg.dispatch("/Crawler/toggle1", handleToggleMessage);
      Serial.println("toggle_MSG");
    }
  }
}
