#if FEATURE_NFC == 1
void initNFC() {
  // todo : objects declaration here ?
  aSerial.v().pln(F("[INIT:NFC]"));
  digitalWrite(NFC_SS_PIN, HIGH);
  pinMode(NFC_SS_PIN, OUTPUT);
#if FEATURE_POWER_MANAGER == 1
  setPowerManagerOn(1, GND_PIN, VCC_PIN_1);
  //  digitalWrite(VCC_PIN_1, HIGH);
#endif
  SPI.begin();
}

void endNFC() {
  aSerial.vv().pln(F("[SLEEP:NFC]"));
  SPI.end(); // shutdown the SPI interface
  pinMode(SCK, INPUT);
  digitalWrite(SCK, LOW); // shut off pullup resistor
  pinMode(MOSI, INPUT);
  digitalWrite(MOSI, LOW); // shut off pullup resistor
  pinMode(MISO, INPUT);
  digitalWrite(MISO, LOW); // shut off pullup resistor
  pinMode(NFC_SS_PIN, INPUT);
  digitalWrite(NFC_SS_PIN, LOW); // shut off pullup resistor
#if FEATURE_POWER_MANAGER == 1
  setPowerManagerOff(1, GND_PIN, VCC_PIN_1);
  //  digitalWrite(VCC_PIN_1, LOW);
#endif
}

void sendNDEF(String payload) {
  aSerial.v().p(F("[TX:NFC] : ")).pln(payload);
  NdefMessage message = NdefMessage();
  message.addTextRecord(payload);
  //  message.addUriRecord("http://test.eu");
  int messageSize = message.getEncodedSize();
  if (messageSize > sizeof(ndefBuf)) {
    aSerial.vv().pln(F("[TX:NFC] ndefBuf is too small"));
    while (1) {
    }
  }
  message.encode(ndefBuf);
  if (0 >= nfc.write(ndefBuf, messageSize)) {
    NDEFSent = false;
    aSerial.vv().pln(F("[TX:NFC] Failed"));
  } else {
    NDEFSent = true;
    aSerial.vv().pln(F("[TX:NFC] Success"));
  }
  delay(2000);
}

void readNDEF() {
  aSerial.vv().pln(F("[RX:NFC]"));
  int msgSize = nfc.read(ndefBuf, sizeof(ndefBuf));
  if (msgSize > 0) {
    NdefMessage message = NdefMessage(ndefBuf, msgSize);
#if DEBUG >= 4
    message.print();
    //  Serial.println(msg._recordCount);
    Serial.println(F("\n[RX:NFC] Success"));
#endif
    // parse the NDEF message
    NdefRecord record = message.getRecord(0);
    int payloadLength = record.getPayloadLength();
    byte payload[payloadLength];
    record.getPayload(payload);
    int startChar = 0;
    if (record.getTnf() == TNF_WELL_KNOWN && record.getType() == "T") { // text message
      // skip the language code
      startChar = payload[0] + 1;
    } else if (record.getTnf() == TNF_WELL_KNOWN && record.getType() == "U") { // URI
      // skip the url prefix (future versions should decode)
      startChar = 1;
    }
    parsing(payloadLength, payload, startChar);
    NDEFRecvd = true;
  } else {
    aSerial.vv().pln(F("[RX:NFC] Failed"));
    NDEFRecvd = false;
  }
  delay(3000);
}

#endif
