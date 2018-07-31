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

#ifdef USE_NEO_SOFTSERIAL   
  #include <NeoSWSerial.h>
#endif

#ifdef USE_SOFTSERIAL   
  #include <SoftwareSerial.h>
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
#endif

#ifdef USE_SOFTSERIAL   
  SoftwareSerial loraSerial(SOFTSERIAL_RX_PIN, SOFTSERIAL_TX_PIN);
#endif

#ifdef FEATURE_LORAWAN
  #include <rn2xx3.h>
  rn2xx3 myLora(loraSerial);
#endif

#include <CayenneLPP.h>
CayenneLPP lpp(PAYLOAD_SIZE);


#ifdef USE_BH1750   
   BH1750 lightMeter;
#endif

#ifdef USE_AM2320   
   AM2320 th;
#endif

#ifdef USE_HCSR04
  NewPing sonar(TRIGGER_PIN, ECHO_PIN, MAX_DISTANCE); // NewPing setup of pins and maximum distance.
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

#ifdef USE_CURRENT_SENSOR   
  // Hold current sum
  double current;
  // Holds the 5 minutes power consumptions, values from 0 to 4 (5 values)
  unsigned int power[SENDING_COUNTS];
#endif  


void blink(byte times, byte mseconds, int pin);
void led_on(int led);
void led_off(int led);
void getBattery(byte instanceId);
void getDigitInput(byte instanceId, int pin);
void getBH1750(byte instanceId);
void getAM2320(byte instanceId);
void getHCSR04(byte instanceId);
double getCurrent(unsigned long samples);
void doRead();
void doStore();
void loraInit();
void doSend();
void doRecv();

// -----------------------------------------------------------------------------
// Utils
// -----------------------------------------------------------------------------

#ifdef USE_NEO_SOFTSERIAL   
  volatile uint32_t newlines = 0UL;
  static void handleRxChar( uint8_t c ) {
    if (c == '\n')
      newlines++;
  }
#endif


void awake() {
    // Nothing to do
}

// Sleeps RN2xx3 at globally defined interval, rectified to keep timing accurate
void sleepRadio() {
  #ifdef FEATURE_DEBUG
      Serial.print(F("[SLEEP RN2483]"));
  #endif
  #ifdef FEATURE_LORAWAN
    myLora.sleep(SLEEP_INTERVAL - millis() + wakeTime);
    if (loraSerial.available()) loraSerial.read();
  #endif
}

// Sleeps MCU and wake it up on radio interrupt 
void sleepController() {
  #ifdef FEATURE_DEBUG
      Serial.print(F("[SLEEP]"));
      //Serial.println();
      //delay(10);
  #endif

  #ifdef FEATURE_INTERRUPT
    attachInterrupt(INTERRUPT_PIN, awake, CHANGE);
    LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);
    wakeTime = millis();
    detachInterrupt(INTERRUPT_PIN);
  #endif 

  #ifdef FEATURE_LORAWAN
    if (loraSerial.available()) {
      loraSerial.read();
      #ifdef FEATURE_DEBUG
        Serial.print(F("[WAKE]"));
      #endif
    }
  #endif
}

// -----------------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------------

void setup() {

  #ifdef FEATURE_DEBUG
    Serial.begin(SERIAL_BAUD);
    Serial.println(F("[ALOES COSMONODE]"));
    Serial.println(F("[SETUP] LoRaWAN Sensor v0.2"));
  #endif
  
  #ifdef FEATURE_POWER_MANAGER
    pinMode(GND_PIN, OUTPUT); 
    pinMode(VCC_PIN_1, OUTPUT);
    digitalWrite(GND_PIN, LOW); 
  #endif

  #ifdef FEATURE_LEDS    
    pinMode(LED_PIN_1, OUTPUT);
    pinMode(LED_PIN_2, OUTPUT);
    led_on(LED_PIN_1);
    led_off(LED_PIN_2);
  #endif

  #ifdef FEATURE_LORAWAN
    #ifdef USE_NEO_SOFTSERIAL
      //loraSerial.attachInterrupt( handleRxChar ); 
    #endif
    loraSerial.begin( SOFTSERIAL_BAUD );
    loraInit();
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
    pinMode(DIGIT_INPUT_PIN_2, INPUT);
  #endif

  #ifdef USE_BH1750
    lightMeter.begin();
  #endif
  // Set initial wake up time
  wakeTime = millis();
  #ifdef FEATURE_LEDS
    led_off(LED_PIN_1);
  #endif 
   //delay(1000);
}

void loop() {
  #ifdef FEATURE_DEBUG
    Serial.println();
    Serial.print(F("[LOOP]"));
  #endif
  #ifdef FEATURE_POWER_MANAGER
    digitalWrite(VCC_PIN_1, HIGH);
  #endif
  if (++count1 == READING_COUNTS) ++count2;
  if (count2 < SENDING_COUNTS) sleepRadio();
  
  #ifdef FEATURE_LEDS
    led_on(LED_PIN_1);
    //blink(1, 5);
  #endif 

  doRead();
  
  if (count1 == READING_COUNTS) doStore();
  if (count2 == SENDING_COUNTS) {
      #ifdef FEATURE_LEDS
        led_on(LED_PIN_2);
        // blink(3, 5, LED_PIN_2);
      #endif
      doSend();
      //delay(SLEEP_INTERVAL - millis() + wakeTime);
      sleepRadio();
      #ifdef FEATURE_LEDS
        led_off(LED_PIN_2);
      #endif
  }
  
  // Overflow counters
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
