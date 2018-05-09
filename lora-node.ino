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

#if FEATURE_LORAWAN == ON
  #include <rn2xx3.h>
  rn2xx3 myLora(loraSerial);
#endif

#ifdef USE_BH1750   
   BH1750 lightMeter;
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

#if FEATURE_LEDS == ON
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

#if FEATURE_BATTERY_SENSOR == ON
  unsigned int getBattery() {
    unsigned int voltage = BATTERY_RATIO * analogRead(BATTERY_PIN);
    #if FEATURE_DEBUG == ON
        Serial.print(F("[MAIN] Battery: "));
        Serial.println(voltage);
    #endif
    return voltage;
  }
#endif

#ifdef USE_DIGIT_INPUT
  unsigned int getDigitInput(int pin) {  
    unsigned int state = digitalRead(pin);
    #if FEATURE_DEBUG == ON
      char buffer[50];
      sprintf(buffer, "[MAIN] Digit Input #%d: %d\n", pin, state);
      Serial.print(buffer);
    #endif
    return state;
  }
#endif

#ifdef USE_BH1750
  unsigned int getBH1750() {  
    delay(50); 
    uint16_t lux = lightMeter.readLightLevel();
    //delay(1000);
    #if FEATURE_DEBUG == ON
      char buffer[50];
      sprintf(buffer, "[MAIN] BH1750 : %d lux\n", lux);
      Serial.print(buffer);
    #endif
    return lux;
  }
#endif

#ifdef USE_AM2320
#endif

#ifdef USE_HCSR04
  unsigned int getHCSR04() { 
    delay(50); 
    unsigned int uS = sonar.ping();
    unsigned int distance = uS / US_ROUNDTRIP_CM;
    #if FEATURE_DEBUG == ON
      char buffer[50];
      sprintf(buffer, "[MAIN] HCSR04 : %d cm\n", distance);
      Serial.print(buffer);
    #endif
    return distance;
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
    #if FEATURE_DEBUG == ON
        Serial.print(F("[MAIN] Current (A): "));
        Serial.println(current);
    #endif
    return current;
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
  #if FEATURE_DEBUG == ON
      Serial.println(F("[MAIN] Sleeping RN2xx3"));
  #endif
  #if FEATURE_LORAWAN == ON
    myLora.sleep(SLEEP_INTERVAL - millis() + wakeTime);
    if (loraSerial.available()) loraSerial.read();
  #endif
}

// Sleeps MCU and wake it up on radio interrupt 
void sleepController() {
  #if FEATURE_DEBUG == ON
      Serial.println(F("[MAIN] Sleeping ATmega"));
      Serial.println();
      delay(15);
  #endif

  #if FEATURE_INTERRUPT == ON
    attachInterrupt(INTERRUPT_PIN, awake, CHANGE);
    LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);
    wakeTime = millis();
    detachInterrupt(INTERRUPT_PIN);
  #endif 

  #if FEATURE_LORAWAN == ON
    if (loraSerial.available()) {
      loraSerial.read();
      #if FEATURE_DEBUG == ON
        Serial.println(F("[MAIN] Awake!"));
      #endif
    }
  #endif
}

// -----------------------------------------------------------------------------
// Actions
// -----------------------------------------------------------------------------

// Reads the sensor value and adds it to the accumulator
void doRead() {
  #ifdef USE_CURRENT_SENSOR
    current = current + getCurrent(CURRENT_SAMPLES);
  #endif
  
  #ifdef USE_DIGIT_INPUT
    unsigned int digitIn1 = getDigitInput(DIGIT_INPUT_PIN_1);
    unsigned int digitIn2 = getDigitInput(DIGIT_INPUT_PIN_2);
  #endif

  #ifdef USE_BH1750
    unsigned int bh1750 = getBH1750();
  #endif

  #ifdef USE_AM2320
    int am2320Temp = getAM2320T();
    int am2320Hum = getAM2320H();
  #endif
  
  #ifdef USE_HCSR04
    unsigned int hcsr04 = getHCSR04();
  #endif
}

// Stores the current average into the minutes array
void doStore() {
    #ifdef USE_CURRENT_SENSOR
       power[count2-1] = (current * MAINS_VOLTAGE) / READING_COUNTS;
      #if FEATURE_DEBUG == ON
          char buffer[50];
          sprintf(buffer, "[MAIN] Storing power in slot #%d: %dW\n", count2, power[count2-1]);
          Serial.print(buffer);
      #endif
      current = 0;
    #endif
}

// Sends the [SENDING_COUNTS] averages
void doSend() {
  byte payload[SENDING_COUNTS * 2 + 4];
    
  #if FEATURE_BATTERY_SENSOR == ON
    unsigned int voltage = getBattery();
    payload[SENDING_COUNTS * 2] = (voltage >> 8) & 0xFF;
    payload[SENDING_COUNTS * 2 + 1] = voltage & 0xFF;
    //    delay(20);
  #endif     
  #ifdef USE_DIGIT_INPUT
    int digitIn1 = getDigitInput(DIGIT_INPUT_PIN_1);
    payload[SENDING_COUNTS * 2] = (digitIn1 >> 8) & 0xFF;
    payload[SENDING_COUNTS * 2 + 1] = digitIn1 & 0xFF;
    int digitIn2 = getDigitInput(DIGIT_INPUT_PIN_2);
    payload[SENDING_COUNTS * 2] = (digitIn2 >> 8) & 0xFF;
    payload[SENDING_COUNTS * 2 + 1] = digitIn2 & 0xFF;
  #endif

  #ifdef USE_BH1750
    int bh1750 = getBH1750();
      payload[SENDING_COUNTS * 2] = (bh1750 >> 8) & 0xFF;
      payload[SENDING_COUNTS * 2 + 1] = bh1750 & 0xFF;
  #endif

  #ifdef USE_AM2320
    int temperature = getAM2320T();
    payload[SENDING_COUNTS * 2] = (temperature >> 8) & 0xFF;
    payload[SENDING_COUNTS * 2 + 1] = temperature & 0xFF; 
    delay(20);
    int humidity = getAM2320H();
    payload[SENDING_COUNTS * 2] = (humidity >> 8) & 0xFF;
    payload[SENDING_COUNTS * 2 + 1] = humidity & 0xFF;
  #endif

  #ifdef USE_HCSR04
    int distance = getHCSR04();
    payload[SENDING_COUNTS * 2] = (distance >> 8) & 0xFF;
    payload[SENDING_COUNTS * 2 + 1] = distance & 0xFF; 
  #endif

  #ifdef USE_CURRENT_SENSOR
    for (int i=0; i<SENDING_COUNTS; i++) {
        payload[i*2] = (power[i] >> 8) & 0xFF;
        payload[i*2+1] = power[i] & 0xFF;
    }
  #endif

  #if FEATURE_LORAWAN == ON
      myLora.txBytes(payload, SENDING_COUNTS * 2 + 4);
  #endif
    
   #if FEATURE_DEBUG == ON
     Serial.println(F("[MAIN] Sending"));
   #endif

}

void doRecv() {
  
   #if FEATURE_LEDS == ON
   
   #endif
   
   #if FEATURE_LORAWAN == ON
     String received = myLora.getRx();
     int signal = myLora.getSNR();
     received = myLora.base16decode(received);
   #endif
   
    #if FEATURE_DEBUG == ON
       Serial.println("[MAIN] Received downlink : " + received);
       Serial.println("[MAIN] Received signal : " + signal);
   #endif
}

// -----------------------------------------------------------------------------
// Radio
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
      #if FEATURE_DEBUG == ON
          Serial.println(F("[ERROR] Communication with RN2xx3 unsuccesful. Power cycle the board."));
          Serial.println(hweui);
      #endif
      delay(10000);
      hweui = myLora.hweui();
    }

    #if FEATURE_DEBUG == ON
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
    #endif
    #ifdef USE_OTAA_AUTH
      join_result = myLora.initOTAA(APPEUI, APPKEY);
    #endif

    while(!join_result) {
      #if FEATURE_DEBUG == ON
         Serial.println(APPKEY);
         Serial.println(F("[ERROR] Unable to join. Are your keys correct, and do you have LoRaWan coverage?"));
      #endif
      delay(30000); 
      join_result = myLora.init();
    }
    myLora.txCnf(SKETCH_NAME); // presentation message
    while (loraSerial.available()) loraSerial.read();
  }


// -----------------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------------

void setup() {

  #if FEATURE_DEBUG == ON
    Serial.begin(SERIAL_BAUD);
    Serial.println(F("[ALOES COSMONODE]"));
    Serial.println(F("[SETUP] LoRaWAN Sensor v0.2"));
  #endif
  
  #if FEATURE_POWER_MANAGER == ON
    pinMode(GND_PIN, OUTPUT); pinMode(VCC_PIN_1, OUTPUT);
  #endif

  #if FEATURE_LEDS == ON
     pinMode(LED_PIN_1, OUTPUT);
     pinMode(LED_PIN_2, OUTPUT);
     led_on(LED_PIN_1);
     led_off(LED_PIN_2);
  #endif
 
  delay(50);
  unsigned int uS = sonar.ping(); // Send ping, get ping time in microseconds (uS).
  Serial.print("Ping: ");
  Serial.print(uS / US_ROUNDTRIP_CM); // Convert ping time to distance and print result (0 = outside set distance range, no ping echo)
  Serial.println("cm");
  delay(50);
  uS = sonar.ping(); // Send ping, get ping time in microseconds (uS).
  Serial.print("Ping: ");
  Serial.print(uS / US_ROUNDTRIP_CM); // Convert ping time to distance and print result (0 = outside set distance range, no ping echo)
  Serial.println("cm");  


  #if FEATURE_LORAWAN == ON
    // Configure radio
    #ifdef USE_NEO_SOFTSERIAL
      loraSerial.attachInterrupt( handleRxChar ); 
    #endif
    loraSerial.begin( SOFTSERIAL_BAUD );
    loraInit();
  #endif

  #if FEATURE_BATTERY_SENSOR == ON
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

   #if FEATURE_LEDS == ON
    led_off(LED_PIN_1);
   #endif
   
   //delay(1000);

}

void loop() {
  
  if (++count1 == READING_COUNTS) ++count2;
  if (count2 < SENDING_COUNTS) sleepRadio();
  
  #if FEATURE_LEDS == ON
    led_on(LED_PIN_1);
    //blink(1, 5);
  #endif 

  #if FEATURE_POWER_MANAGER == ON
    digitalWrite(GND_PIN, LOW); digitalWrite(VCC_PIN_1, HIGH);
  #endif

  
  doRead();
  
  if (count1 == READING_COUNTS) doStore();
  if (count2 == SENDING_COUNTS) {
      #ifdef FEATURE_LEDS
        led_on(LED_PIN_2);
        // blink(3, 5, LED_PIN_2);
      #endif
      doSend();
      sleepRadio();
      #ifdef FEATURE_LEDS
        led_off(LED_PIN_2);
      #endif
  }
  
  // Overflow counters
  count1 %= READING_COUNTS;
  count2 %= SENDING_COUNTS;
    
  #if FEATURE_LEDS == ON
    led_off(LED_PIN_1);
  #endif

   #if FEATURE_POWER_MANAGER == ON
    digitalWrite(GND_PIN, LOW); digitalWrite(VCC_PIN_1, LOW);
  #endif
   
  #if FEATURE_INTERRUPT == ON
     sleepController();
  #else 
    delay(SLEEP_INTERVAL);
  #endif
  
}
