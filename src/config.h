#ifndef _CONFIG_H_
#define _CONFIG_H_

#define uS_TO_S_FACTOR 1000000ULL  // Used to determine how long to deep sleep the ESP32


// Network must be the same (14) for all of my units to communicate w/ each other.
#define LORA_NETWORK_ID 14

// Used to set the recipient address for all LoRa transmissions.
// My base station's address is 2200. Valid transmitter addresses are 2201-2240.
#define LORA_BASE_STATION_ADDRESS 2200UL

// Un-comment and change the baud rate below to change it.
// #define LORA_BAUD_RATE 115200ULL     // default 115200

// Configure each of the variables below for each transmitter

String TRANSMITTER_NAME = "Wx";
#define NRD_INTERVAL 900 // Non-Rain Data Interval. 900 is 15 minutes
#define LORA_NODE_ADDRESS 2206UL // Bessie=2201, Boat=2202, Test=2203, Pool=2204, Garden=2205, Wx=2206
#define R1_VALUE 100500.0 // actual measured value
#define R2_VALUE 22040.0  // ditto
// This will be different for each transmitter device, and must be calculated from actual
// measurements taken of the source voltage, to get the final voltage correct. Calibrate
// at normal battery voltage for known input voltage.
#define VOLTAGE_CALIBRATION 0.98  // Calculated 2/8/2023

#define HIGH_VOLTAGE_ALARM_VALUE 14.55 //Victron solar controller is set to 14.5
#define HIGH_VOLTAGE_ALARM_CODE 3
#define HIGH_VOLTAGE_EMAIL_INTERVAL 15 // In MINUTES
#define HIGH_VOLTAGE_MAX_EMAILS 3

#define LOW_VOLTAGE_ALARM_VALUE 13.10 // 13.0 is about 30% for a LiFePO4 (but that's a rough estimate)
#define LOW_VOLTAGE_ALARM_CODE 1 // Not an urgent alarm
#define LOW_VOLTAGE_EMAIL_INTERVAL 240 // In MINUTES (4 hours)
#define LOW_VOLTAGE_MAX_EMAILS 5

#define RAIN_DATA_INTERVAL 60 // 60 is 1 minute
#define RAIN_BUCKET_VOLUME 100.0 // BAS: measure the volume of each dump of the bucket
#define RAIN_SENSE_DURATION 3 // Multiples of RAIN_DATA_INTERVAL. How long to stay awake after it seems to have stopped raining

#endif // #ifndef _CONFIG_H_
