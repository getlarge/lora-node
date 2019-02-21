# Aloes - LoraWan Node

- Send and receive LoraWan packet via RN2483 chip, using softwareSerial to communicate with Arduino board.
- Authentification by OTAA and ABP via Aloes lorawan-transport or any other LoraWan Server.
- Decode payload received/sent with CayenneLPP.
- Support configuration via NFC ( PN532 ) :
	- OTAA mode : send DevEUI and AppEUI, wait for AppKey
	- ABP mode : send devAddr and AppEUI, wait for AppSKey and Nwk


LoraWan MQTT API build upon :
- [Arduino](https://www.arduino.cc/)
- [RN2xx3 library](https://github.com/jpmeijers/RN2483-Arduino-Library)
- [CayenneLPP](https://github.com/sabas1080/CayenneLPP)
- [PN532 NFC library](https://github.com/adafruit/Adafruit-PN532)
- [LoraWan transport](https://framagit.org/aloes/lorawan-transport)

-----


## Configuration

Edit your config in `config.h.sample` and save it as `config.h`.
You can override these settings by activating NFC ( FEATURE_NFC = 1 ) 

