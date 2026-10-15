// Process joystick input and control motors
void processJoystickInput(int16_t joyX, int16_t joyY) {
  // Map joystick values to motor speeds
  // Using tank drive control scheme (differential steering)

static bool didBrake = false;
static bool isReverse = false;

static int aboveDeadZoneCount = 0;
static int belowDeadZoneCount = 0;
static int reverseCount = 0;
static bool breakLight = false;
static unsigned long breakLightStart;

  // Calculate motor speeds based on joystick position
  // This creates a mixing function for tank-style control
  int16_t leftMotorSpeed = joyY + joyX;
  int16_t rightMotorSpeed = joyY - joyX;
  
  if (crawlerType == CRAWLER_PCA) {
    // Crawler 1: Using PCA9685
    // Map from joystick range to PWM range
    leftMotorSpeed = map(leftMotorSpeed, -512, 512, -PWM_MAX, PWM_MAX);
    rightMotorSpeed = map(rightMotorSpeed, -512, 512, -PWM_MAX, PWM_MAX);
    if (abs(leftMotorSpeed) < MOTOR_DEADZONE) leftMotorSpeed = 0;
    if (abs(rightMotorSpeed) < MOTOR_DEADZONE) rightMotorSpeed = 0;
    // Set motor speeds
    setMotorPCA(MOTOR1_PCA_PIN1, MOTOR1_PCA_PIN2, leftMotorSpeed);
    setMotorPCA(MOTOR2_PCA_PIN1, MOTOR2_PCA_PIN2, rightMotorSpeed);
    
    bool isCurrentlyAboveDeadZone = abs(leftMotorSpeed) > MOTOR_DEADZONE || abs(rightMotorSpeed) > MOTOR_DEADZONE;
    bool isCurrentlyBelowDeadZone = abs(leftMotorSpeed) <= MOTOR_DEADZONE && abs(rightMotorSpeed) <= MOTOR_DEADZONE;
    bool isCurrentlyReverse = -leftMotorSpeed < -MOTOR_DEADZONE && -rightMotorSpeed < -MOTOR_DEADZONE;
    if ((leftMotorSpeed > MOTOR_DEADZONE && rightMotorSpeed < MOTOR_DEADZONE) || (leftMotorSpeed < MOTOR_DEADZONE && rightMotorSpeed > MOTOR_DEADZONE)) isCurrentlyAboveDeadZone = false;

  // Check whether the joystick was above the deadzone twice in a row
    if (isCurrentlyAboveDeadZone) {
      aboveDeadZoneCount++;
      belowDeadZoneCount = 0; // reset
    }
   const int limit = 2;
   // Check whether the joystick was below the deadzone twice in a row
    if (isCurrentlyBelowDeadZone) {
     belowDeadZoneCount++;
     if (belowDeadZoneCount > limit) aboveDeadZoneCount = 0; // reset
    }

    // didBrake logic (twice above deadzone followed by twice below deadzone)
    if (aboveDeadZoneCount >= limit && belowDeadZoneCount >= limit) {
      didBrake = true;
      // Reset counters after the event
      aboveDeadZoneCount = 0;
      belowDeadZoneCount = 0;
    } else {
      didBrake = false;
    }

     // isReverse logic (reverse twice in a row)
    if (isCurrentlyReverse) {
      reverseCount++;
    } else {
      reverseCount = 0;
    }

    if (reverseCount >= 2) {
      isReverse = true;
    } else {
      isReverse = false;
    }
   //Serial << leftMotorSpeed << "   " << rightMotorSpeed << endl;
   //Serial << isCurrentlyAboveDeadZone << "  " << isCurrentlyBelowDeadZone  << "  "  << isCurrentlyReverse << endl;
   //Serial << belowDeadZoneCount  << "  " << aboveDeadZoneCount << "  " <<  reverseCount << endl;
   //Serial << didBrake << "   " << isReverse << endl;
   
   if (didBrake && !breakLight) {
      breakLight = true;
      breakLightStart = millis();
      setRedLight(2000);
    }
    if (breakLight && millis() > (breakLightStart + breakLightDuration)) {
      breakLight = false;
      setRedLight(0);
    }   
    if (isReverse) setWhiteLight(400); else setWhiteLight(0);
  } 
  else if (crawlerType == CRAWLER_GPIO) {
    // Crawler 2: Using direct GPIO
    // Map from joystick range to GPIO PWM range
    leftMotorSpeed = map(leftMotorSpeed, -512, 512, -GPIO_PWM_MAX, GPIO_PWM_MAX);
    rightMotorSpeed = map(rightMotorSpeed, -512, 512, -GPIO_PWM_MAX, GPIO_PWM_MAX);
    // Set motor speeds
#if defined ESP8266
    setMotorDirect(MOTOR1_GPIO_PIN1, MOTOR1_GPIO_PIN2, leftMotorSpeed);
    setMotorDirect(MOTOR2_GPIO_PIN1, MOTOR2_GPIO_PIN2, rightMotorSpeed);
#elif defined ESP32
    setMotorDirect(MOTOR1_CHAN1, MOTOR1_CHAN2, rightMotorSpeed); //leftMotorSpeed
    setMotorDirect(MOTOR2_CHAN1, MOTOR2_CHAN2, leftMotorSpeed); //rightMotorSpeed
#endif
  }
}

// Function to stop all motors
void stopMotors() {
  processJoystickInput(0,0);
}

// Function to set motor speed and direction using PCA9685
void setMotorPCA(uint8_t pin1, uint8_t pin2, int16_t speed) {
  // Constrain speed to valid range
  speed = constrain(speed, -PWM_MAX, PWM_MAX);
  // Apply deadzone
  if (abs(speed) < MOTOR_DEADZONE) {
    speed = 0;
  } 
  if (speed >= 0) {
    // Forward
    pwm.setPWM(pin1, 0, speed);
    pwm.setPWM(pin2, 0, 0);
  } else {
    // Reverse
    pwm.setPWM(pin1, 0, 0);
    pwm.setPWM(pin2, 0, -speed);
  }
}

// Function to set motor speed and direction using direct GPIO pins
void setMotorDirect(uint8_t pin1, uint8_t pin2, int16_t speed) {
  // Constrain speed to valid range
  speed = constrain(speed, -GPIO_PWM_MAX, GPIO_PWM_MAX); 
#if defined ESP8266
  // Apply deadzone
  if (abs(speed) < MOTOR_DEADZONE) {
    speed = 0;
    analogWrite(pin1, 0);
    analogWrite(pin2, 0);
    return;
  } 
  if (speed >= 0) {
    // Forward
    analogWrite(pin1, GPIO_PWM_MAX - speed);
    digitalWrite(pin2, HIGH);
  } else {
    // Reverse
    digitalWrite(pin1, HIGH);
    analogWrite(pin2, GPIO_PWM_MAX + speed);
  }
#elif defined ESP32
  #if ESP_ARDUINO_VERSION_MAJOR >= 3
    #define ledcWrite ledcWriteChannel  // core 3.x: ledcWrite() takes a pin, we pass channels
  #endif
/*
  if (speed > 0) {
    ledcWrite(pin1, speed);  // actually a channel
    ledcWrite(pin2, 0); //0
  } else if (speed < 0) {
    ledcWrite(pin1, 0);
    ledcWrite(pin2, -speed);
  } else {  // Standstill
    ledcWrite(pin1, 0);
    ledcWrite(pin2, 0);
  }
 */
  if (speed > 0) {
    //Serial << speed << endl;
    ledcWrite(pin2, GPIO_PWM_MAX - speed);  // actually a channel
    ledcWrite(pin1, GPIO_PWM_MAX); //0
  } else if (speed < 0) {
    ledcWrite(pin2, GPIO_PWM_MAX);
    ledcWrite(pin1, GPIO_PWM_MAX + speed);
  } else {  // Standstill
    ledcWrite(pin1, 0);
    ledcWrite(pin2, 0);
  }
#endif
}
