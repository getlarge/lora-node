#if FEATURE_POWER_MANAGER == 1
void setPowerManagerOn(int instance, int gnd_pin, int vcc_pin) {
  pinMode(gnd_pin, OUTPUT);
  pinMode(vcc_pin, OUTPUT);
  digitalWrite(gnd_pin, LOW);
  pinMode(vcc_pin, HIGH);
}

void setPowerManagerOff(int instance, int gnd_pin, int vcc_pin) {
  pinMode(gnd_pin, OUTPUT);
  pinMode(vcc_pin, OUTPUT);
  digitalWrite(gnd_pin, LOW);
  pinMode(vcc_pin, LOW);
}
#endif

void awake() {
  aSerial.v().pln(F("[WAKE:MCU]"));
  // Nothing to do
}

// Sleeps RN2xx3 at globally defined interval, rectified to keep timing accurate
void sleepRadio(int interval) {
  aSerial.v().pln(F("[SLEEP:LORA]"));
#if FEATURE_LORAWAN == 1
  myLora.sleep(interval - millis() + wakeTime);
  if (loraSerial.available()) loraSerial.read();
#endif
}

// Sleeps MCU and wake it up on radio interrupt
void sleepController() {
  aSerial.v().pln(F("[SLEEP:MCU]"));
  //Serial.println();
  //delay(10);
#ifdef FEATURE_INTERRUPT
  attachInterrupt(INTERRUPT_PIN, awake, CHANGE);
  LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);
  wakeTime = millis();
  detachInterrupt(INTERRUPT_PIN);
#endif

#if FEATURE_LORAWAN == 1
  if (loraSerial.available()) {
    loraSerial.read();
    aSerial.v().pln(F("[WAKE:LORA]"));
  }
#endif
}

void parsing(int payloadLength, byte* payload, int startChar) {
  aSerial.vv().p(F("[PARSING] ")).pln((char*)payload);
  aSerial.vvvv().p(F("[PARSING] payload length : ")).p(payloadLength).p(" | first char : ").pln(startChar);
#ifdef USE_ABP_AUTH
  for (int c = startChar; c < 36; c++) {
    nwksKey += (char)payload[c];
  }
  nwksKey.trim();
  for (int c = 36; c < payloadLength; c++) {
    appsKey += (char)payload[c];
  }
  appsKey.trim();
#elif defined(USE_OTAA_AUTH)
  for (int c = startChar; c < startChar + 17; c++) {
    appEui += (char)payload[c];
  }
  appEui.trim();
  for (int c =  startChar + 17; c < payloadLength; c++) {
    appKey += (char)payload[c];
  }
  appKey.trim();
#endif
}

#if FEATURE_SAVING >= 1
void saveCredentials(char *stringIn) {
  aSerial.vv().p(F("[STORING:EEPROM] credentials, ")).p(F("StringIn Length : ")).p(strlen(stringIn)).pln();
  for (int i = 0; i < strlen(stringIn); i++) {
    EEPROM.write(i, stringIn[i]);
  }
  savedCreds = true;
}

//void readCredentials(char *bufferIn) {
void readCredentials(byte* bufferIn) {
  savedCreds = true;
  for (int i = 0; i < EEPROM_BUFFER_SIZE; i++) {
    bufferIn[i] = EEPROM.read(i);
  }
  int payloadLength = strlen((char*)bufferIn);
  aSerial.vv().pln(F("[READING:EEPROM] credentials "));
  aSerial.vvvv().p(F("BufferIn  : ")).p((char*)bufferIn).p("sizeof : ").p(sizeof(bufferIn)).p(F(" | length : ")).p(payloadLength).pln();
  parsing(payloadLength, bufferIn, 1);
}

void doSave() {
  byte eeprom_buffer[EEPROM_BUFFER_SIZE];
  char first_eeprom_value;
  String creds;
  char credentials[EEPROM_BUFFER_SIZE];
  aSerial.vv().pln(F("[SAVING CREDENTIALS]"));
#ifdef USE_ABP_AUTH
  creds = "~" + nwksKey + " " + appsKey;
#elif defined(USE_OTAA_AUTH)
  creds = "~" + appEui + " " + appKey;
  //  last_eeprom_value = EEPROM.read(52);
#endif
  creds.toCharArray(credentials, sizeof(credentials));
  //  creds = "";
  aSerial.vvv().p(F("Credentials : ")).pln(credentials);
  first_eeprom_value = EEPROM.read(0);
  if (first_eeprom_value != '~') {
    aSerial.vv().pln(F(" No credentials saved in eeprom... "));
    saveCredentials(credentials);
    // int credentialsLenght = 
    //  saveCredentials(credentials, credentialsLenght);
  } else {
    readCredentials(eeprom_buffer);
  }
}
#endif

// Reads the sensor value and adds it to the accumulator
void doRead() {
#if FEATURE_CAYENNE == 1
  lpp.reset();
#endif
  byte instanceId = 0;
  aSerial.vv().pln(F("[READINGS]"));
#ifdef FEATURE_BATTERY_SENSOR
  ++instanceId; // todo : also update payload size
  getBattery(instanceId);
#endif
#ifdef USE_DIGIT_INPUT
  ++instanceId;
  getDigitInput(instanceId, DIGIT_INPUT_PIN_1);
#endif
#ifdef USE_BH1750
  ++instanceId;
  getBH1750(instanceId);
#endif
#ifdef USE_AM2320
  ++instanceId;
  getAM2320(instanceId);
#endif
#ifdef USE_HCSR04
  ++instanceId;
  getHCSR04(instanceId);
#endif
#ifdef USE_CURRENT_SENSOR
  current = current + getCurrent(CURRENT_SAMPLES);
#endif
}

// Stores the current average into the minutes array
void doStore() {
  aSerial.vv().pln(F("[STORING]"));
#ifdef USE_CURRENT_SENSOR
  power[count2 - 1] = (current * MAINS_VOLTAGE) / READING_COUNTS;
  aSerial.v().p(F("[MAIN] Storing power in slot")).p(count2).p(" : ").pln(power[count2 - 1]);
  current = 0;
#endif
}
