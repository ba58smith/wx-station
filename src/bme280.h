#ifndef _BME280_H_
#define _BME280_H_

#include "Arduino.h"
#include "config.h"
#include <Adafruit_BME280.h>

class BME280Sensor {
private:
    Adafruit_BME280 bme280;

public:
    // Constructor. 
    BME280Sensor() {
        Adafruit_BME280 bme280;
        bool success = bme280.begin(0x76);
        if (!success) {
          Serial.println("Could not find a valid BME280 sensor, check wiring!");
        }
        else {
            Serial.println("BME280::begin() was successful");
        }
    }

    float reported_temp() {
       return (bme280.readTemperature() * 1.8) + 32.0; // convert from C to F
    }

    float reported_baro_press() {
       return (bme280.readPressure() * 0.0002953); // convert from Pascals to inches of mercury
    }

    float reported_humidity() {
       return bme280.readHumidity();
    }

};

#endif // _BME280_H_
