// -----------------------------------------------------------------------------
// Configuration
// -----------------------------------------------------------------------------

#define DEBUG

// GPIO Configutation
#define LED_PIN             8
#define INTERRUPT_PIN       1
#define BATTERY_PIN         A3
#define CURRENT_PIN         A1

// Communications
#define SERIAL_BAUD         57600
#define SOFTSERIAL_RX_PIN   3
#define SOFTSERIAL_TX_PIN   4
#define SOFTSERIAL_BAUD     9600
#define RN2483_RESET_PIN    5

// LoRaWAN Configuration
//#define ABP_AUTH // ABP authentification method
#define OTAA_AUTH // OTAA authentification method ( preferred )
#define DEVADDR         "06c48998"
#define APPSKEY         "682e3616a00dcc63bb5fbee858d252b1"
#define NWKSKEY         "c99ffc93dde11a1b755e570f4837c69b"
#define APPEUI          "70B3D57ED0007A5B"
#define APPKEY          "4f786e825a2ab789fbeba1a30a0aafd9"

// Sleep intervals of x milliseconds ( default = 10000 )
#define SLEEP_INTERVAL      10000

// Perform a sensor average reading every (x * SLEEP_INTERVAL ) ms (default = 6)
#define READING_COUNTS      6

// Send readings after x * READING_COUNTS ( default = 5 )
#define SENDING_COUNTS      5

// For a ECS1030-L72 CT
// Specs: 2000 turns ratio (T) without built-in burdenresistor
// Using 68 ohm burden resistor (R)
// A/V = T/R = 2000 * 68 = 29.41
// Real life comparisons showed a 10% bias with this value
// So I am using a correction factor
//#define CURRENT_RATIO       26.20

// For a YHDC SCT013
// Specs: 30A/1V
#define CURRENT_RATIO       30.00

// Number of samples to take for each reading
#define CURRENT_SAMPLES     1000

// Mains voltage to calculate apparent power
#define MAINS_VOLTAGE       230

// ADC precission
#define ADC_BITS            10
#define ADC_COUNTS          (1 << ADC_BITS)

// ADC reference volatge in V
#define ADC_REFERENCE       3.3

// Battery monitoring circuitry:
// Vi -- R1 -- A1 (Vo) -- R2 -- D12 (GND)
//
// These values have been measured with a multimeter:
// R1 = 470K
// R2 = 470k
//
// Formulae:
// Vo = Vi * R2 / (R1 + R2)
// Vi = Vo * (R1 + R2) / R2
// Vo = X * 3300 / 1024;
//
// Thus:
// Vi = X * 3300 * (470 + 470) / 1024 / 470;
#define BATTERY_RATIO       6.45
