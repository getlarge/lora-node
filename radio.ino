// -----------------------------------------------------------------------------
// Radio
// -----------------------------------------------------------------------------
  void loraInit() {
    pinMode(RN2483_RESET_PIN, OUTPUT);
    digitalWrite(RN2483_RESET_PIN, LOW);
    delay(500);
    digitalWrite(RN2483_RESET_PIN, HIGH);
    loraSerial.flush();
    myLora.autobaud();
    myLora.setFrequencyPlan(DEFAULT_EU);
    //check communication with radio
    String hweui = myLora.hweui();
    while(hweui.length() != 16) {
      #ifdef FEATURE_DEBUG
          Serial.println(F("[ERROR] Communication with RN2xx3 unsuccesful. Power cycle the board."));
          Serial.println(hweui);
      #endif
      delay(10000);
      hweui = myLora.hweui();
    }
    #ifdef FEATURE_DEBUG
       //print out the HWEUI so that we can register it ( next : send HWEUI via NFC ? )
        Serial.println(F("[SETUP] When using OTAA, register this DevEUI: "));
        Serial.print(F("[SETUP] "));
        Serial.println(myLora.hweui());
        Serial.println(F("[SETUP] RN2xx3 firmware version:"));
        Serial.print(F("[SETUP] "));
        Serial.println(myLora.sysver());
        Serial.println(F("[SETUP] Trying to join LoRaWan Server"));
    #endif
    
    bool join_result = false;
    #ifdef USE_ABP_AUTH
      join_result = myLora.initABP(DEVADDR, APPSKEY, NWKSKEY);
    #elif defined(USE_OTAA_AUTH)
      join_result = myLora.initOTAA(APPEUI, APPKEY);
    #endif

    while(!join_result) {
      #ifdef FEATURE_DEBUG
         Serial.println(APPKEY);
         Serial.println(F("[ERROR] Unable to join. Are your keys correct, and do you have LoRaWan coverage?"));
      #endif
      delay(JOIN_DURATION); 
      join_result = myLora.init();
    }
    #ifdef FEATURE_DEBUG
      Serial.println(F("[SUCCESS] Joined LoRaWan network"));
    #endif
    //myLora.txCnf(SKETCH_NAME); // presentation message, send payload frame to define handler
//    while (loraSerial.available()) loraSerial.read();
  }
  
// Sends the [SENDING_COUNTS] averages
void doSend() {

//  #ifdef USE_CURRENT_SENSOR
//    for (int i=0; i<SENDING_COUNTS; i++) {
//        payload[i*2] = (power[i] >> 8) & 0xFF;
//        payload[i*2+1] = power[i] & 0xFF;
//    }
//  #endif

  #ifdef FEATURE_POWER_MANAGER
    digitalWrite(VCC_PIN_1, LOW);
  #endif
  
  #ifdef FEATURE_LORAWAN
      //Serial.print("lppBuffer: ");
      //Serial.println((const char*)lpp.getBuffer());
      #ifdef FEATURE_DEBUG
        Serial.print(F("[SENDING]"));
      #endif
      myLora.txBytes(lpp.getBuffer(), lpp.getSize());
      //myLora.txCnf("test");
  #endif
    
   #ifdef FEATURE_DEBUG
     Serial.print(F("[SENT]"));
   #endif

}

void doRecv() {
  #ifdef FEATURE_DEBUG
    Serial.print(F("[RECEIVED] "));
  #endif
  
  #ifdef FEATURE_LEDS 
  #endif
   
   #ifdef FEATURE_LORAWAN
    String received = myLora.getRx();
    int signal = myLora.getSNR();
    received = myLora.base16decode(received);
    //lpp.addDigitalOutput(4, received);    // channel 4, set digital output high
    //lpp.addAnalogOutput(5, 120.0); // channel 5, set analog output to 120
   #endif
   
    #ifdef FEATURE_DEBUG
       Serial.println("[RECV] : " + received);
       Serial.println("[MAIN] Received signal : " + signal);
   #endif
}
