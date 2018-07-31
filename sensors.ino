
#ifdef FEATURE_LEDS
  void blink(byte times, byte mseconds, int pin) {
      pinMode(pin, OUTPUT);
      for (byte i=0; i<times; i++) {
          if (i>0) delay(mseconds);
          digitalWrite(pin, HIGH);
          delay(mseconds);
          digitalWrite(pin, LOW);
      }
   // pinMode(LED_PIN, INPUT);
  }

   void led_on(int led) {
    digitalWrite(led, 1);
  }
  
  void led_off(int led) {
    digitalWrite(led, 0);
  }
#endif

// -----------------------------------------------------------------------------
// Readings
// -----------------------------------------------------------------------------

#ifdef FEATURE_BATTERY_SENSOR
  void getBattery(byte instanceId) {
    unsigned int voltage = BATTERY_RATIO * analogRead(BATTERY_PIN);
    #ifdef FEATURE_DEBUG
        Serial.print(F("[3203/%d] Battery: "));
        Serial.println(voltage);
    #endif
    if (count2 == SENDING_COUNTS) {
      lpp.addAnalogOutput(instanceId, voltage);
    }
    //return voltage;
  }
#endif

#ifdef USE_DIGIT_INPUT
  //unsigned int getDigitInput(int pin) {  
  void getDigitInput(byte instanceId, int pin) {  
    unsigned int state = digitalRead(pin);
    #ifdef FEATURE_DEBUG
      char buffer[50];
      sprintf(buffer, "[3200/%d] #%d: %d | ", instanceId, pin, state);
      Serial.print(buffer);
    #endif
    //return state;
    if (count2 == SENDING_COUNTS) {
      lpp.addDigitalInput(instanceId, state);
    }
  }
#endif

#ifdef USE_BH1750
  void getBH1750(int instanceId) {  
    delay(50); 
    uint16_t lux = lightMeter.readLightLevel();
    //delay(1000);
    #ifdef FEATURE_DEBUG
      char buffer[50];
      sprintf(buffer, "[3301/%d]: %d | ", instanceId, lux);
      Serial.print(buffer);
    #endif
    if (count2 == SENDING_COUNTS) {
      lpp.addLuminosity(instanceId, lux);
    }
    //return lux;
  }
#endif

#ifdef USE_AM2320
  void getAM2320(byte instanceId) {
    th.Read();
    //Serial.println(th.Read());
    switch(th.Read()) {
      case 2:
        Serial.println("CRC failed");
        break;
      case 1:
        Serial.println("Sensor offline");
        break;
      case 0:
      if (count2 == SENDING_COUNTS) {
        lpp.addTemperature(instanceId, th.t);
        lpp.addRelativeHumidity(instanceId+1, th.h);
      }
        #ifdef FEATURE_DEBUG
          char buffer[50];
          sprintf(buffer, "[3303/%d] : %d | ", instanceId, th.t);
          Serial.print(buffer);
          sprintf(buffer, "[3304/%d] : %d | ", instanceId+1, th.h);
          Serial.print(buffer);
          //sprintf(buffer, "[MAIN] AM2320 : %d C - %d % \n", th.t, th.h);
        #endif
        break;
    }
  }
#endif

#ifdef USE_HCSR04
   void getHCSR04(byte instanceId) { 
    delay(50); 
    unsigned int uS = sonar.ping();
    unsigned int distance = uS / US_ROUNDTRIP_CM;
    #ifdef FEATURE_DEBUG
      char buffer[50];
      sprintf(buffer, "[MAIN] HCSR04 : %d cm\n", distance);
      Serial.print(buffer);
    #endif
    //return distance;
  }
#endif

#ifdef USE_CURRENT_SENSOR
  // Based on EmonLib calcIrms method
  double getCurrent(unsigned long samples) {
    static double offset = (ADC_COUNTS >> 1);
    unsigned long sum = 0;
    for (unsigned int n = 0; n < samples; n++) {
        unsigned long reading = analogRead(CURRENT_PIN);
        // Digital low pass
        offset = ( offset + ( reading - offset ) / 1024 );
        unsigned long filtered = reading - offset;
        // Root-mean-square method current
        sum += ( filtered * filtered );
    }
    double current = CURRENT_RATIO * sqrt(sum / samples) * ADC_REFERENCE / ADC_COUNTS;
    #ifdef FEATURE_DEBUG
        Serial.print(F("[MAIN] Current (A): "));
        Serial.print(current);
    #endif
    return current;
  }
#endif


// -----------------------------------------------------------------------------
// Actions
// -----------------------------------------------------------------------------

// Reads the sensor value and adds it to the accumulator
void doRead() {
  lpp.reset();
  byte instanceId = 0;
  #ifdef FEATURE_DEBUG
    Serial.print(F("[READINGS] "));
  #endif
    
  #ifdef FEATURE_BATTERY_SENSOR
    ++instanceId; // todo : also update payload size
    getBattery(instanceId);
  #endif  
     
  #ifdef USE_DIGIT_INPUT
    ++instanceId;
    getDigitInput(instanceId, DIGIT_INPUT_PIN_1);
    //int digitIn2 = getDigitInput(DIGIT_INPUT_PIN_2);
  #endif

  #ifdef USE_BH1750
    ++instanceId;
    getBH1750(instanceId);
  #endif

  #ifdef USE_AM2320
    ++instanceId;
    getAM2320(instanceId);
  #endif
//
  #ifdef USE_HCSR04
    ++instanceId;
    getHCSR04(instanceId);
  #endif
//

  #ifdef USE_CURRENT_SENSOR
    current = current + getCurrent(CURRENT_SAMPLES);
  #endif


}

// Stores the current average into the minutes array
void doStore() {
    Serial.print(F("[STORING] "));
    #ifdef USE_CURRENT_SENSOR
       power[count2-1] = (current * MAINS_VOLTAGE) / READING_COUNTS;
      #ifdef FEATURE_DEBUG
          char buffer[50];
          sprintf(buffer, "[MAIN] Storing power in slot #%d: %dW\n", count2, power[count2-1]);
          Serial.print(buffer);
      #endif
      current = 0;
    #endif
}
