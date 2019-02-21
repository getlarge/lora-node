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

//  void loraSetup(rn2xx3 myLora(loraSerial)) {
void loraSetup() {
  yield();
  Serial.flush();
  aSerial.vv().p(F("[JOIN:LORA] "));
  bool join_result = false;
  myLora.setFrequencyPlan(TTN_EU);
  aSerial.vv().p(F("LoRaWAN set frequencies :")).pln(myLora.setFrequencyPlan(TTN_EU));

#ifdef USE_ABP_AUTH
  aSerial.vv().pln(F("ABP AUTH ..."));
  aSerial.vvv().p(F("Network session key : ")).pln(nwksKey).p(F("App session key : ")).pln(appsKey);
  join_result = myLora.initABP(DEVADDR, appsKey, nwksKey);
  //  join_result = myLora.initABP(DEVADDR, (String)appsKey, (String)nwksKey);
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
  //  String message = myLora.base16encode(SKETCH_NAME);
    sendStringMsg( "JOINED!");
  //  while (loraSerial.available()) loraSerial.read();
}

void sendStringMsg(String message) {
  switch (myLora.txCnf(message)) //one byte, blocking function
  {
    case TX_FAIL:
      {
        Serial.println("TX unsuccessful or not acknowledged");
        break;
      }
    case TX_SUCCESS:
      {
        Serial.println("TX successful and acknowledged");
        break;
      }
    case TX_WITH_RX:
      {
        String received = myLora.getRx();
        received = myLora.base16decode(received);
        Serial.print("Received downlink: " + received);
        break;
      }
    default:
      {
        Serial.println("Unknown response from TX function");
      }
  }
}
#endif
