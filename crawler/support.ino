void setLed(uint8_t channel, uint16_t brightness) {
    if (brightness > 4095) brightness = 4095; // Limit to maximum value
    pwm.setPWM(channel, 0, brightness);
}

void setRightRedLight(int delta) {
  setLed(redRight,stdBrightnessRed + delta);
}

void setLeftRedLight(int delta) {
  setLed(redLeft,stdBrightnessRed + delta);
}
void setRedLight(int delta) {
  setRightRedLight(delta);
  setLeftRedLight(delta);
}

void setWhiteLight(int delta) {
  setLed(whiteLeft,0 + delta);
  setLed(whiteRight,0 + delta); 
}

void setFrontLight(int delta) {
  setLed(frontLeft,stdBrightnessFrontLeft + delta);
  setLed(frontRight,stdBrightnessFrontRight + 3*delta);
}

void ledsOff() {
  if (crawlerType == CRAWLER_PCA) {
    setLed(redLeft,0);
    setLed(redRight,0);
    setLed(frontLeft,0);
    setLed(frontRight,0); 
    setWhiteLight(3000); 
  }
  else if (crawlerType == CRAWLER_GPIO) {
    digitalWrite(wifiLed,LOW); 
  }
}


uint8_t detectCrawlerType() {
  Wire.beginTransmission(0x40); // Default PCA9685 address
  byte error = Wire.endTransmission(); 
  if (error == 0) {
    // PCA9685 found
    Serial.println("PCA9685 detected - this is Crawler Type 1");
    return CRAWLER_PCA;
  } else {
    // No PCA9685, assume direct GPIO
    Serial.println("No PCA9685 detected - this is Crawler Type 2");
    return CRAWLER_GPIO;
  }
}
