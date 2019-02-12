/*

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

// -----------------------------------------------------------------------------
// Libraries
// -----------------------------------------------------------------------------

#include "config.h"
#include <LowPower.h>
#include <advancedSerial.h>

#ifdef USE_NEO_SOFTSERIAL
#include <NeoSWSerial.h>
#elif defined(USE_SOFTSERIAL)
#include <SoftwareSerial.h>
#endif

#if FEATURE_NFC == 1
#include "SPI.h"
#include "PN532_SPI.h"
#include "snep.h"
#include "NdefMessage.h"
#endif

#if FEATURE_SAVING >= 1
#include <EEPROM.h>
#endif

#ifdef USE_BH1750
#include <Wire.h>
#include <BH1750.h>
#endif

#ifdef USE_AM2320
#include <Wire.h>
#include <AM2320.h>
#endif

#ifdef USE_HCSR04
#include <NewPing.h>
#endif

// -----------------------------------------------------------------------------
// Objects declaration
// -----------------------------------------------------------------------------

#ifdef USE_NEO_SOFTSERIAL
NeoSWSerial loraSerial( SOFTSERIAL_RX_PIN, SOFTSERIAL_TX_PIN );
#elif defined(USE_SOFTSERIAL)
SoftwareSerial loraSerial(SOFTSERIAL_RX_PIN, SOFTSERIAL_TX_PIN);
#endif

#if FEATURE_LORAWAN == 1
#include <rn2xx3.h>
rn2xx3 myLora(loraSerial);
#endif

#if FEATURE_CAYENNE == 1
#include <CayenneLPP.h>
CayenneLPP lpp(PAYLOAD_SIZE);
#endif

#if FEATURE_NFC == 1
PN532_SPI pn532spi(SPI, NFC_SS_PIN);
SNEP nfc(pn532spi);
uint8_t ndefBuf[NFC_BUFFER_SIZE];
#endif

#ifdef USE_BH1750
BH1750 lightMeter;
#endif

#ifdef USE_AM2320
AM2320 th;
#endif

#ifdef USE_HCSR04
// NewPing setup of pins and maximum distance.
NewPing sonar(TRIGGER_PIN, ECHO_PIN, MAX_DISTANCE);
#endif

// -----------------------------------------------------------------------------
// Global variables
// -----------------------------------------------------------------------------

// Hold the last time we wake up, to calculate how much we have to sleep next time
unsigned long wakeTime = millis();
// Counts the one minute parcials, values from 0 to 5 (6 parcials)
byte count1 = 0;
// Counts the 5 minutes power consumptions, values from 0 to 4 (5 values)
byte count2 = 0;

/// UTILS
#if FEATURE_POWER_MANAGER == 1
void setPowerManagerOn(int instance, int gnd_pin, int vcc_pin);
void setPowerManagerOff(int instance, int gnd_pin, int vcc_pin);
#endif
void sleepRadio(int interval);
void sleepController();
void awake();
void doRead();
void doStore();
void parsing(int payloadLength, byte* payload, int startChar);
#if FEATURE_SAVING >= 1
void doSave();
void saveCredentials(char *stringIn);
void readCredentials(byte *bufferIn);
#endif
#ifdef FEATURE_LEDS
void blink(byte times, byte mseconds, int pin);
void led_on(int led);
void led_off(int led);
#endif

/// SENSORS
#ifdef FEATURE_BATTERY_SENSOR
void getBattery(byte instanceId);
#endif
#ifdef USE_DIGIT_INPUT
void getDigitInput(byte instanceId, int pin);
#endif
#ifdef USE_BH1750
void getBH1750(byte instanceId);
#endif
#ifdef USE_AM2320
void getAM2320(byte instanceId);
#endif
#ifdef USE_HCSR04
void getHCSR04(byte instanceId);
#endif
#ifdef USE_CURRENT_SENSOR
double getCurrent(unsigned long samples);
double current;
// Holds the 5 minutes power consumptions, values from 0 to 4 (5 values)
unsigned int power[SENDING_COUNTS];
#endif

/// LORA - RN2xx3
#if FEATURE_LORAWAN == 1
void loraInit();
void loraSetup();
void doSend();
void doRecv();
bool joined = false;
#endif

/// NFC - PN532
#if FEATURE_NFC == 1
void initNFC();
void endNFC();
void sendNDEF(String payload);
void readNDEF();
#endif
bool NDEFSent = false;
bool NDEFRecvd = false;
bool savedCreds = false;

// -----------------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------------

void setup() {
  Serial.begin(SERIAL_BAUD);
#if DEBUG != 0
  aSerial.setPrinter(Serial);
#elif DEBUG == 0
  aSerial.off();
  //Serial.setDebugOutput(false);
#endif
#if DEBUG == 1
  aSerial.setFilter(Level::v);
#elif DEBUG == 2
  aSerial.setFilter(Level::vv);
#elif DEBUG == 3
  aSerial.setFilter(Level::vvv);
#elif DEBUG == 4
  aSerial.setFilter(Level::vvvv);
#endif
  aSerial.v().p(F("====== ")).p(SKETCH_NAME).p(" v").p(SKETCH_VERSION).pln(F(" ======"));
  aSerial.v().pln(F("====== [SETUP STARTED] ======"));


#ifdef FEATURE_LEDS
  pinMode(LED_PIN_1, OUTPUT);
  pinMode(LED_PIN_2, OUTPUT);
  led_on(LED_PIN_1);
  led_off(LED_PIN_2);
#endif
#ifdef FEATURE_BATTERY_SENSOR
  pinMode(BATTERY_PIN, INPUT);
#endif
#ifdef USE_CURRENT_SENSOR
  pinMode(CURRENT_PIN, INPUT);
  // Warmup the current monitoring
  getCurrent(CURRENT_SAMPLES * 3);
#endif
#ifdef USE_DIGIT_INPUT
  pinMode(DIGIT_INPUT_PIN_1, INPUT);
  //  pinMode(DIGIT_INPUT_PIN_2, INPUT);
#endif
#ifdef USE_BH1750
  lightMeter.begin();
#endif
#if FEATURE_SAVING == 2
  // clear eeprom on new flash... just for debugging
  aSerial.vv().pln(F("[RESET:EEPROM]"));
  for (int i = 0 ; i < EEPROM.length() ; i++) {
    EEPROM.write(i, 0);
  }
#elif FEATURE_SAVING == 1
  byte eeprom_buffer[EEPROM_BUFFER_SIZE];
  char first_eeprom_value;
  first_eeprom_value = EEPROM.read(0);
  if (first_eeprom_value != '~') {
    aSerial.vv().pln(F("[INIT:EEPROM] No value saved in eeprom..."));
  } else {
    readCredentials(eeprom_buffer);
  }
  //  delay(1000);

#endif

#if FEATURE_NFC == 1
  if (savedCreds == false) {
    initNFC();
    //  delay(100);
  }
  else if (savedCreds == true) {
    endNFC();
    delay(100);
  }
#endif
#if FEATURE_LORAWAN == 1
  loraSerial.begin( SOFTSERIAL_BAUD );
  loraInit();
#endif

  // Set initial wake up time
  wakeTime = millis();
#ifdef FEATURE_LEDS
  led_off(LED_PIN_1);
#endif
  aSerial.v().pln(F("====== [SETUP ENDED] ======"));
}

void loop() {
  aSerial.v().pln("");
  aSerial.v().pln(F("====== [LOOP] ======"));
  if ( joined == false && NDEFSent == false && savedCreds == false ) {
    // retrieve LoRaWan credentials via NFC P2P communications
    while ( NDEFSent != true ) {
#ifdef USE_ABP_AUTH
      sendNDEF(DEVADDR);
#elif defined(USE_OTAA_AUTH)
      sendNDEF(hweui);
#endif
    }
  }
  else if ( joined == false && NDEFSent == true && NDEFRecvd == false) {
#if FEATURE_NFC == 1
    readNDEF();
#endif
  }
  else if (joined == false && NDEFSent == true && NDEFRecvd == true) {
#if FEATURE_NFC == 1
    endNFC();
    delay(100);
#endif
#if FEATURE_SAVING == 1
    doSave();
#endif
#if FEATURE_LORAWAN == 1
    loraSetup();
#endif
  }
  else if (joined == false && savedCreds == true) {
#if FEATURE_LORAWAN == 1
    loraSetup();
#endif
  }
  else if (joined == true) {
    if (++count1 == READING_COUNTS) ++count2;
    if (count2 < SENDING_COUNTS) sleepRadio(SLEEP_INTERVAL);
#ifdef FEATURE_LEDS
    led_on(LED_PIN_1);
    //  blink(1, 5);
#endif
#if FEATURE_POWER_MANAGER == 1
    setPowerManagerOn(1, GND_PIN, VCC_PIN_1);
#endif
    doRead();
#if FEATURE_POWER_MANAGER == 1
    setPowerManagerOff(1, GND_PIN, VCC_PIN_1);
#endif
    if (count1 == READING_COUNTS) doStore();
    if (count2 == SENDING_COUNTS) {
#ifdef FEATURE_LEDS
      led_on(LED_PIN_2);
      // blink(3, 5, LED_PIN_2);
#endif
#if FEATURE_LORAWAN == 1
      doSend();
      sleepRadio(SLEEP_INTERVAL);
#endif
#ifdef FEATURE_LEDS
      led_off(LED_PIN_2);
#endif
    }
    count1 %= READING_COUNTS;
    count2 %= SENDING_COUNTS;
#ifdef FEATURE_LEDS
    led_off(LED_PIN_1);
#endif
#ifdef FEATURE_INTERRUPT
    sleepController();
#else
    delay(SLEEP_INTERVAL);
#endif
  }
}
