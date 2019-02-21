
#ifdef FEATURE_LEDS
void blink(byte times, byte mseconds, int pin) {
  pinMode(pin, OUTPUT);
  for (byte i = 0; i < times; i++) {
    if (i > 0) delay(mseconds);
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
  //Serial.print(F("[3203/%d] Battery: "));
  aSerial.v().p(F("[MAIN] 3203/")).p(instanceId).p(" ( V ) : ")).pln(voltage);
  if (count2 == SENDING_COUNTS) {
#if FEATURE_CAYENNE == 1
  lpp.addAnalogOutput(instanceId, voltage);
#endif
  }
  //return voltage;
}
#endif

#ifdef USE_DIGIT_INPUT
void getDigitInput(byte instanceId, int pin) {
  unsigned int state = digitalRead(pin);
  aSerial.v().p(F("[MAIN] 3200/")).p(instanceId).p(F(" | pin: ")).p(pin).p(F(" | state : ")).pln(state);
  //return state;
#if FEATURE_CAYENNE == 1
  lpp.addDigitalInput(instanceId, state);
#endif

}
#endif


#ifdef USE_ANALOG_INPUT
void getAnalogInput(byte instanceId, int pin) {
  float value = analogRead(pin);
  aSerial.v().p(F("[MAIN] 3202/")).p(instanceId).p(F(" | pin: ")).p(pin).p(F(" | value : ")).pln(value);
  //return state;
#if FEATURE_CAYENNE == 1
  lpp.addAnalogInput(instanceId, value);
#endif

}
#endif

#ifdef USE_BH1750
void getBH1750(byte instanceId) {
  delay(50);
  uint16_t lux = lightMeter.readLightLevel();
  //delay(1000);
  aSerial.v().p(F("[MAIN] 3301/")).p(instanceId).p(F(" ( lux ) : ")).pln(lux);
#if FEATURE_CAYENNE == 1
  lpp.addLuminosity(instanceId, lux);
#endif
}
#endif

#ifdef USE_AM2320
void getAM2320(byte instanceId) {
  th.Read();
  //Serial.println(th.Read());
  switch (th.Read()) {
    case 2:
      aSerial.v().pln(F("[AM2320] CRC Failed"));
      break;
    case 1:
      aSerial.v().pln(F("[AM2320] Offline"));
      break;
    case 0:
#if FEATURE_CAYENNE == 1
      lpp.addTemperature(instanceId, th.t);
      lpp.addRelativeHumidity(instanceId + 1, th.h);
#endif
      aSerial.v().p(F("[MAIN] 3303/")).p(instanceId).p(F(" ( °C ) : ")).pln(th.t);
      aSerial.v().p(F("[MAIN] 3304/")).p(instanceId).p(F(" ( % ) : ")).pln(th.h);
      break;
  }
}
#endif

#ifdef USE_HCSR04
void getHCSR04(byte instanceId) {
  delay(50);
  unsigned int uS = sonar.ping();
  unsigned int distance = uS / US_ROUNDTRIP_CM;
  aSerial.v().p(F("[MAIN] HCSR04 (cm) : ")).pln(distance);
#if FEATURE_CAYENNE == 1
#endif
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
  aSerial.v().p(F("[MAIN] Current (A) : ")).pln(current);
#if FEATURE_CAYENNE == 1
#endif
  return current;
}
#endif
