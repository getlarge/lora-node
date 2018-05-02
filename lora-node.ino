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

//#define NEO_SOFTSERIAL   
#define SOFTSERIAL 
  
#define LEDS
#define POWER_MANAGER
#define DIGIT_INPUT
#define DIGIT_OUTPUT
#define ANALOG_INPUT
//#define BH1750
//#define AM2320
#define BATTERY_SENSOR
#define CURRENT_SENSOR

  #include <Wire.h>
  #include <BH1750.h>
  
#include "config.h"

//#include <Arduino.h>

#include <LowPower.h>

#ifdef NEO_SOFTSERIAL   
  #include <NeoSWSerial.h>
  NeoSWSerial loraSerial( SOFTSERIAL_RX_PIN, SOFTSERIAL_TX_PIN );
#endif

#ifdef SOFTSERIAL   
  #include <SoftwareSerial.h>
  SoftwareSerial loraSerial(SOFTSERIAL_RX_PIN, SOFTSERIAL_TX_PIN);
#endif

#include <rn2xx3.h>
rn2xx3 myLora(loraSerial);

#ifdef BH1750

   BH1750 lightMeter;
#endif

// -----------------------------------------------------------------------------
// Globals
// -----------------------------------------------------------------------------

// Hold the last time we wake up, to calculate how much we have to sleep next time
unsigned long wakeTime = millis();
// Hold current sum
double current;
// Counts the one minute parcials, values from 0 to 5 (6 parcials)
byte count1 = 0;
// Counts the 5 minutes power consumptions, values from 0 to 4 (5 values)
byte count2 = 0;
// Holds the 5 minutes power consumptions, values from 0 to 4 (5 values)
unsigned int power[SENDING_COUNTS];


#ifdef BH1750
#endif

// -----------------------------------------------------------------------------
// Utils
// -----------------------------------------------------------------------------

#ifdef NEO_SOFTSERIAL   
  volatile uint32_t newlines = 0UL;
  
  static void handleRxChar( uint8_t c ) {
    if (c == '\n')
      newlines++;
  }
#endif

#ifdef LEDS
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

#ifdef CURRENT_SENSOR
  // Based on EmonLib calcIrms method
  double getCurrent(unsigned long samples) {

    static double offset = (ADC_COUNTS >> 1);
    unsigned long sum = 0;

    for (unsigned int n = 0; n < samples; n++) {

        // Get the reading
        //unsigned long reading = analogRead(CURRENT_PIN);
        unsigned long reading = analogRead(BATTERY_PIN);

        // Digital low pass
        offset = ( offset + ( reading - offset ) / 1024 );
        unsigned long filtered = reading - offset;

        // Root-mean-square method current
        sum += ( filtered * filtered );

    }

    double current = CURRENT_RATIO * sqrt(sum / samples) * ADC_REFERENCE / ADC_COUNTS;
    #ifdef DEBUG
        Serial.print(F("[MAIN] Current (A): "));
        Serial.println(current);
    #endif
    return current;
  
  }
#endif

#ifdef BATTERY_SENSOR
  unsigned int getBattery() {
    unsigned int voltage = BATTERY_RATIO * analogRead(BATTERY_PIN);
    #ifdef DEBUG
        Serial.print(F("[MAIN] Battery: "));
        Serial.println(voltage);
    #endif
    return voltage;
  }
#endif

#ifdef DIGIT_INPUT
  unsigned int getDigitInput(int pin) {  
    unsigned int state = digitalRead(pin);
    #ifdef DEBUG
        char buffer[50];
        sprintf(buffer, "[MAIN] Digit Input #%d: %d\n", pin, state);
        Serial.print(buffer);
    #endif
    return state;
  }
#endif

// -----------------------------------------------------------------------------
// Sleeping
// -----------------------------------------------------------------------------

void awake() {
    // Nothing to do
}

// Sleeps RN2xx3 at globally defined interval, rectified to keep timing accurate
void sleepRadio() {
    #ifdef DEBUG
        Serial.println(F("[MAIN] Sleeping RN2xx3"));
    #endif
    myLora.sleep(SLEEP_INTERVAL - millis() + wakeTime);
    
    if (loraSerial.available()) loraSerial.read();
}

// Sleeps MCU and wake it up on radio interrupt 
void sleepController() {
    #ifdef DEBUG
        Serial.println(F("[MAIN] Sleeping ATmega"));
        Serial.println();
        delay(15);
    #endif
    attachInterrupt(INTERRUPT_PIN, awake, CHANGE);
    LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);
    wakeTime = millis();
    detachInterrupt(INTERRUPT_PIN);

    if (loraSerial.available()) {
      loraSerial.read();
      #ifdef DEBUG
        Serial.println(F("[MAIN] Awake!"));
      #endif
    }
}

// -----------------------------------------------------------------------------
// Actions
// -----------------------------------------------------------------------------

// Reads the sensor value and adds it to the accumulator
void doRead() {
    #ifdef CURRENT_SENSOR
      current = current + getCurrent(CURRENT_SAMPLES);
      delay(50);
    #endif
    
    #ifdef DIGIT_INPUT
      int digitIn1 = getDigitInput(DIGIT_INPUT_PIN_1);
      delay(50);
      int digitIn2 = getDigitInput(DIGIT_INPUT_PIN_2);
    #endif
}

// Stores the current average into the minutes array
void doStore() {
    #ifdef CURRENT_SENSOR
    power[count2-1] = (current * MAINS_VOLTAGE) / READING_COUNTS;
    #endif

    #ifdef DEBUG
        char buffer[50];
        sprintf(buffer, "[MAIN] Storing power in slot #%d: %dW\n", count2, power[count2-1]);
        Serial.print(buffer);
    #endif
    current = 0;
}

// Sends the [SENDING_COUNTS] averages
void doSend() {

  byte payload[SENDING_COUNTS * 2 + 4];
    
  #ifdef CURRENT_SENSOR
    for (int i=0; i<SENDING_COUNTS; i++) {
        payload[i*2] = (power[i] >> 8) & 0xFF;
        payload[i*2+1] = power[i] & 0xFF;
    }
  #endif
  
  #ifdef BATTERY_SENSOR
    unsigned int voltage = getBattery();
    payload[SENDING_COUNTS * 2] = (voltage >> 8) & 0xFF;
    payload[SENDING_COUNTS * 2 + 1] = voltage & 0xFF;
    //    delay(20);
  #endif     
  

  #ifdef AM2320
    int temperature = getAM2320T();
    payload[SENDING_COUNTS * 2] = (temperature >> 8) & 0xFF;
    payload[SENDING_COUNTS * 2 + 1] = temperature & 0xFF; 
    delay(20);
    int humidity = getAM2320H();
    payload[SENDING_COUNTS * 2] = (humidity >> 8) & 0xFF;
    payload[SENDING_COUNTS * 2 + 1] = humidity & 0xFF;
  #endif
   
    delay(50);
    
  #ifdef DIGIT_INPUT
    int digitIn1 = getDigitInput(DIGIT_INPUT_PIN_1);
    payload[SENDING_COUNTS * 2] = (digitIn1 >> 8) & 0xFF;
    payload[SENDING_COUNTS * 2 + 1] = digitIn1 & 0xFF;
    delay(50);
    int digitIn2 = getDigitInput(DIGIT_INPUT_PIN_2);
    payload[SENDING_COUNTS * 2] = (digitIn2 >> 8) & 0xFF;
    payload[SENDING_COUNTS * 2 + 1] = digitIn2 & 0xFF;
  #endif

    myLora.txBytes(payload, SENDING_COUNTS * 2 + 4);
    
    #ifdef DEBUG
        Serial.println(F("[MAIN] Sending"));
    #endif

}

void doRecv() {
   #ifdef LEDS
   
   #endif
   String received = myLora.getRx();
   int signal = myLora.getSNR();
   received = myLora.base16decode(received);
   #ifdef DEBUG
       Serial.println("[MAIN] Received downlink : " + received);
       Serial.println("[MAIN] Received signal : " + signal);
   #endif
}

// -----------------------------------------------------------------------------
// RN2483
// -----------------------------------------------------------------------------

void loraInit() {
  
    pinMode(RN2483_RESET_PIN, OUTPUT);
    digitalWrite(RN2483_RESET_PIN, LOW);
    delay(50);
    digitalWrite(RN2483_RESET_PIN, HIGH);
    delay(50);
    loraSerial.flush();
    myLora.autobaud();
    
    //check communication with radio
    String hweui = myLora.hweui();
    while(hweui.length() != 16) {
      #ifdef DEBUG
          Serial.println(F("[ERROR] Communication with RN2xx3 unsuccesful. Power cycle the board."));
          Serial.println(hweui);
      #endif
      delay(10000);
      hweui = myLora.hweui();
    }

    #ifdef DEBUG
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
    
    #ifdef ABP_AUTH
      join_result = myLora.initABP(DEVADDR, APPSKEY, NWKSKEY);
    #endif
    
    #ifdef OTAA_AUTH
      join_result = myLora.initOTAA(APPEUI, APPKEY);
    #endif

    while(!join_result) {
      #ifdef DEBUG
         Serial.println(APPKEY);
         Serial.println(F("[ERROR] Unable to join. Are your keys correct, and do you have LoRaWan coverage?"));
      #endif
      delay(30000); 
      join_result = myLora.init();
    }

    myLora.txCnf(SKETCH_NAME);

    while (loraSerial.available()) loraSerial.read();
}

// -----------------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------------

void setup() {
  
  #ifdef LEDS
     pinMode(LED_PIN_1, OUTPUT);
     pinMode(LED_PIN_2, OUTPUT);
     led_on(LED_PIN_1);
     led_off(LED_PIN_2);
  #endif
 
  #ifdef DEBUG
    Serial.begin(SERIAL_BAUD);
    Serial.println(F("[ALOES COSMONODE]"));
    Serial.println(F("[SETUP] LoRaWAN Sensor v0.2"));
  #endif

  #ifdef CURRENT_SENSOR
    pinMode(CURRENT_PIN, INPUT);
  #endif
  
  #ifdef BATTERY_SENSOR
    pinMode(BATTERY_PIN, INPUT);
  #endif
  
  #ifdef DIGIT_INPUT
    pinMode(DIGIT_INPUT_PIN_1, INPUT);
    pinMode(DIGIT_INPUT_PIN_2, INPUT);
  #endif
  
  #ifdef POWER_MANAGER
    pinMode(GND_PIN, OUTPUT); pinMode(VCC_PIN_1, OUTPUT);
  #endif

  #ifdef CURRENT_SENSOR
    // Warmup the current monitoring
    getCurrent(CURRENT_SAMPLES * 3);
  #endif
  
    // Configure radio
    //loraSerial.attachInterrupt( handleRxChar ); // Due to NeoSW library 
    loraSerial.begin( SOFTSERIAL_BAUD );
    loraInit();

    // Set initial wake up time
    wakeTime = millis();

   #ifdef LEDS
    led_off(LED_PIN_1);
   #endif
   
   delay(1000);

}

void loop() {

  #ifdef LEDS
    led_on(LED_PIN_1);
  #endif 

  #ifdef POWER_MANAGER
    digitalWrite(GND_PIN, LOW); digitalWrite(VCC_PIN_1, HIGH);
  #endif
  
  // Update counters
  if (++count1 == READING_COUNTS) ++count2;

  // We are only sending if both counters have overflown
  // so to save power we shutoff the radio now if no need to send
  if (count2 < SENDING_COUNTS) sleepRadio();

  // Visual notification
  //blink(1, 5);

  // Always perform a new reading
  doRead();

  // We are storing it if count1 has overflown
  if (count1 == READING_COUNTS) doStore();

  // We are only sending if both counters have overflow
  if (count2 == SENDING_COUNTS) {
      #ifdef LEDS
        led_on(LED_PIN_2);
      #endif
     // blink(3, 5, LED_PIN_2);
      doSend();
      sleepRadio();
      #ifdef LEDS
        led_off(LED_PIN_2);
      #endif
  }
  
  // Overflow counters
  count1 %= READING_COUNTS;
  count2 %= SENDING_COUNTS;
    
  #ifdef LED
    led_off(LED_PIN_1);
  #endif

   #ifdef POWER_MANAGER
    digitalWrite(GND_PIN, LOW); digitalWrite(VCC_PIN_1, LOW);
  #endif
  
  // Sleep the controller, the radio will wake it up
  sleepController();
  

}
