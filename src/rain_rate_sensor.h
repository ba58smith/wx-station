#ifndef _RAIN_RATE_SENSOR_H_
#define _RAIN_RATE_SENSOR_H_

#include "Arduino.h"
#include "config.h"
#include "analog_reader.h"
#include "elapsedMillis.h"

class RainRateSensor {
private:
    uint16_t dump_counter_;

public:
    // Constructor for the water volume sensor instance. 
    RainRateSensor() {}

    /**
     * @brief Takes the number of dumps of the rain bucket, the volume of
     * each dump, and the time period, and calculates the rate of rain per hour.
     */
    float reported_rain_rate(uint16_t dump_counter, elapsedMillis rain_measurement_time) {
        float rain_rate = dump_counter * RAIN_BUCKET_VOLUME / RAIN_DATA_INTERVAL;//BAS: calculate rain rate / hour 
        return rain_rate;
    }
};

#endif // _RAIN_RATE_SENSOR_H_
