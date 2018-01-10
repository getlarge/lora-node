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

#include <Arduino.h>
#include <NeoSWSerial.h>    
//#include <SoftwareSerial.h>
#include <rn2xx3.h>
#include <LowPower.h>
#include <Wire.h>
#include <AM2320.h>

#include "config.h"

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

NeoSWSerial loraSerial( SOFTSERIAL_RX_PIN, SOFTSERIAL_TX_PIN );
//SoftwareSerial loraSerial(SOFTSERIAL_RX_PIN, SOFTSERIAL_TX_PIN);
rn2xx3 myLora(loraSerial);

AM2320 th;

// -----------------------------------------------------------------------------
// Utils
// -----------------------------------------------------------------------------

volatile uint32_t newlines = 0UL;

static void handleRxChar( uint8_t c ) {
  if (c == '\n')
    newlines++;
}

void blink(byte times, byte mseconds) {
    pinMode(LED_PIN, OUTPUT);
    for (byte i=0; i<times; i++) {
        if (i>0) delay(mseconds);
        digitalWrite(LED_PIN, HIGH);
        delay(mseconds);
        digitalWrite(LED_PIN, LOW);
    }
   // pinMode(LED_PIN, INPUT);
}

void led_on() {
  digitalWrite(LED_PIN, 1);
}

void led_off() {
  digitalWrite(LED_PIN, 0);
}

// -----------------------------------------------------------------------------
// Readings
// -----------------------------------------------------------------------------

// Based on EmonLib calcIrms method
double getCurrent(unsigned long samples) {

    static double offset = (ADC_COUNTS >> 1);
    unsigned long sum = 0;

    for (unsigned int n = 0; n < samples; n++) {

        // Get the reading
        unsigned long reading = analogRead(CURRENT_PIN);

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

unsigned int getBattery() {
    unsigned int voltage = BATTERY_RATIO * analogRead(BATTERY_PIN);
    #ifdef DEBUG
        Serial.print(F("[MAIN] Battery: "));
        Serial.println(voltage);
    #endif
    return voltage;
}

int getAM2320T() {
    int temperature;
    th.Read();
    switch(th.Read()) {
      case 2:
        Serial.println(F("[ERROR] CRC failed"));
        break;
      case 1:
        Serial.println(F("[ERROR] Sensor offline"));
        break;
      case 0:
        temperature = (th.t  * 10);
    #ifdef DEBUG
        Serial.print(F("[MAIN] Temperature: (°C) "));
        Serial.println(temperature);
    #endif
        break;
    }
    return temperature;
}

int getAM2320H() {
    int humidity;
    th.Read();
    switch(th.Read()) {
      case 2:
        Serial.println(F("[ERROR] CRC failed"));
        break;
      case 1:
        Serial.println(F("[ERROR] Sensor offline"));
        break;
      case 0:
         humidity = (th.h * 10);
    #ifdef DEBUG
        Serial.print(F("[MAIN] Humidity (%): "));
        Serial.println(humidity);
    #endif
        break;
    }
    return humidity;
}


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
    current = current + getCurrent(CURRENT_SAMPLES);
}

// Stores the current average into the minutes array
void doStore() {
    power[count2-1] = (current * MAINS_VOLTAGE) / READING_COUNTS;
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

    for (int i=0; i<SENDING_COUNTS; i++) {
        payload[i*2] = (power[i] >> 8) & 0xFF;
        payload[i*2+1] = power[i] & 0xFF;
    }

    unsigned int voltage = getBattery();
    payload[SENDING_COUNTS * 2] = (voltage >> 8) & 0xFF;
    payload[SENDING_COUNTS * 2 + 1] = voltage & 0xFF;
       
//    delay(20);

//    int temperature = getAM2320T();
//    payload[SENDING_COUNTS * 2] = (temperature >> 8) & 0xFF;
//    payload[SENDING_COUNTS * 2 + 1] = temperature & 0xFF;
        
//    delay(20);

//    int humidity = getAM2320H();
//    payload[SENDING_COUNTS * 2] = (humidity >> 8) & 0xFF;
//    payload[SENDING_COUNTS * 2 + 1] = humidity & 0xFF;

    myLora.txBytes(payload, SENDING_COUNTS * 2 + 2);
    
    #ifdef DEBUG
        Serial.println(F("[MAIN] Sending"));
    #endif

}

void doRecv() {
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
         Serial.println(F("[ERROR] Unable to join. Are your keys correct, and do you have LoRaWan coverage?"));
      #endif
      delay(30000); 
      join_result = myLora.init();
    }

    myLora.tx("Say Hello");

    //while (loraSerial.available()) loraSerial.read();
}

// -----------------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------------

void setup() {
  
   pinMode(LED_PIN, OUTPUT);
   pinMode(10, OUTPUT);
   led_on();
   
    #ifdef DEBUG
        Serial.begin(SERIAL_BAUD);
        Serial.println(F("[ALOES COSMONODE]"));
        Serial.println(F("[SETUP] LoRaWAN Sensor v0.2"));
    #endif

    pinMode(CURRENT_PIN, INPUT);
    pinMode(BATTERY_PIN, INPUT);

    // Warmup the current monitoring
    getCurrent(CURRENT_SAMPLES * 3);

    // Configure radio
    
    //loraSerial.attachInterrupt( handleRxChar ); // Due to NeoSW library 
    loraSerial.begin( SOFTSERIAL_BAUD );
    loraInit();

    // Set initial wake up time
    wakeTime = millis();
       
    led_off();
    
    delay(1000);

}

void loop() {
  
    led_on();

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
        blink(3, 5);
        doSend();
        sleepRadio();
    }

    // Overflow counters
    count1 %= READING_COUNTS;
    count2 %= SENDING_COUNTS;
    
    led_off();
    
    // Sleep the controller, the radio will wake it up
    sleepController();

}
