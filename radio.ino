// -----------------------------------------------------------------------------
// Radio
// -----------------------------------------------------------------------------
#if FEATURE_LORAWAN == 1
void loraInit() {
  // todo : objects declaration here ?
  aSerial.v().pln(F("[INIT:LORA]"));
  pinMode(RN2483_RESET_PIN, OUTPUT);
  digitalWrite(RN2483_RESET_PIN, LOW);
  delay(50);
  digitalWrite(RN2483_RESET_PIN, HIGH);
  loraSerial.flush();
  myLora.autobaud();
  aSerial.vvv().p(F("RN2xx3 firmware version : ")).pln(myLora.sysver());
#ifdef USE_OTAA_AUTH
  myLora.setFrequencyPlan(TTN_EU);
#endif
  //  check communication with radio
  hweui = myLora.hweui();
  // String hweui = myLora.hweui();
  while (hweui.length() != 16) {
    aSerial.vv().pln(F("[ERROR] Communication with RN2xx3 unsuccesful. Power cycle the board."));
    delay(10000);
    hweui = myLora.hweui();
  }
#ifdef USE_ABP_AUTH
  aSerial.vv().p(F("Register this DevAddress : ")).pln(DEVADDR);
#elif defined(USE_OTAA_AUTH)
  aSerial.vv().p(F("Register this DevEUI : ")).pln(hweui);
#endif

#if FEATURE_NFC == 1
  // continue config into the loop
#elif FEATURE_NFC == 0
  // enter into LoRaWan session with hardcoded credentials
  loraSetup();
#endif
}

void loraSetup() {
  yield();
  Serial.flush();
  aSerial.vv().p(F("[JOIN:LORA] "));
  bool join_result = false;
#ifdef USE_ABP_AUTH
  aSerial.vv().pln(F("ABP AUTH ..."));
  aSerial.vvv().p(F("Network session key : ")).pln(nwksKey).p(F("App session key : ")).pln(appsKey);
  join_result = myLora.initABP(DEVADDR, appsKey, nwksKey);
#elif defined(USE_OTAA_AUTH)
  aSerial.vv().pln(F("OTAA AUTH ..."));
  aSerial.vvv().p(F("App EUI : ")).pln(appEui).p(F("App Key : ")).pln(appKey);
  join_result = myLora.initOTAA((String)appEui, (String)appKey);
  //  join_result = myLora.initOTAA("70B3D57ED0007A5B", "4F786E825A2AB789FBEBA1A30A0AAFD7");
#endif

  while (!join_result) {
    //  joinCounter++;
    delay(JOIN_DURATION);
    aSerial.vv().pln(F("[ERROR] Unable to join. Are your keys correct, and do you have LoRaWan coverage?"));
    join_result = myLora.init();
  }
  aSerial.vv().pln(F("[SUCCESS] Joined LoRaWan network"));
  joined = true; 
  //  myLora.txCnf(SKETCH_NAME); // presentation message, send payload frame to define handler
  //  while (loraSerial.available()) loraSerial.read();
}

// Sends the [SENDING_COUNTS] averages
void doSend() {
  aSerial.vv().pln(F("[TX:LORA] ..."));
  //  #ifdef USE_CURRENT_SENSOR
  //    for (int i=0; i<SENDING_COUNTS; i++) {
  //        payload[i*2] = (power[i] >> 8) & 0xFF;
  //        payload[i*2+1] = power[i] & 0xFF;
  //    }
  //  #endif
#if FEATURE_POWER_MANAGER == 1
  digitalWrite(VCC_PIN_1, LOW);
#endif
#if FEATURE_CAYENNE == 1
  aSerial.v().pln(F("[TX:LORA] ... "));
  myLora.txBytes(lpp.getBuffer(), lpp.getSize());
#elif FEATURE_CAYENNE == 0
  myLora.txCnf("test msg");
#endif
  aSerial.vv().pln(F("[TX:LORA] Done"));
}

void doRecv() {
  aSerial.v().pln(F("[RX:LORA]"));
#ifdef FEATURE_LEDS

#endif
  String received = myLora.getRx();
  int signal = myLora.getSNR();
  received = myLora.base16decode(received);
#if FEATURE_CAYENNE == 1
  //  lpp.addDigitalOutput(4, received);    // channel 4, set digital output high
  //  lpp.addAnalogOutput(5, 120.0); // channel 5, set analog output to 120
#endif
  aSerial.v().pln(F("[RX:LORA] Payload :")).p(received);
  aSerial.vvv().pln(F("[RX:LORA] Received signal : ")).p(signal);
}

#endif
