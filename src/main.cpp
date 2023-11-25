// Use board "ESP32 DEVKIT V1"
/* 
This is the version of the LoRa Transmitter that transmits data
from the Weather Station.
Data sent: Rain rate, Water level (in the canal), battery voltage,
sun intensity, temperature, humidity, barometric pressure.
*/

#include <Arduino.h>
#include "functions.h"
#include "reyax_lora.h"
#include "analog_reader.h"
#include "water_volume_sensor.h"
#include "rain_rate_sensor.h"
#include "elapsedMillis.h"

/**
 * Before building, look at all of the #define options in config.h. At the very least,
 * make sure you un-comment the correct #define for the name of the transmitter that this code is
 * going to be used for.
 * 
 * If you change the #defined transmitter or the NETWORK_ID (in config.h), or, if it's the first time
 * installing this firmware on a new ESP32:
 * Un-comment "#define LORA_SETUP_REQUIRED" below, upload and run once, then comment out "#define LORA_SETUP_REQUIRED",
 * and upload and run again. That will prevent writing the NETWORK_ID and NODE_ADDRESS to EEPROM every run.
 */
// #define LORA_SETUP_REQUIRED

uint8_t hall_sensor_pin = 32;
uint8_t voltage_measurement_pin = 33;
uint8_t solar_sensor_pin = 34;
uint8_t water_level_pin = 35;
// var to keep track of how many times the tipping bucket has dumped since the rain started
static RTC_DATA_ATTR uint16_t rain_counter = 0;
uint16_t last_rain_counter = 0;
uint8_t periods_without_rain = 0;
uint32_t time_in_deep_sleep_seconds;
uint32_t data_send_timer;
elapsedMillis data_submit_timer = 0;
elapsedMillis rain_data_submit_timer = 0;

/* Variable to store the time at which deep sleep is entered. Used to
   calculate the length of time in deep sleep, if the wakeup happens
   because of the Hall Sensor interrupt (it started to rain).
*/
static RTC_DATA_ATTR struct timeval sleep_enter_time;

// To stop the auto-fill with the float switch, as a fail-safe
// Stored in IRAM (Internal RAM) for maximum speed of loading and execution
void IRAM_ATTR its_raining_isr() {
  rain_counter++;
}

ReyaxLoRa lora(0);
VoltageSensor voltage_sensor(voltage_measurement_pin);
WaterVolumeSensor water_volume_sensor(water_level_pin);
RainRateSensor rain_sensor;

void setup() {  
  Serial.begin(115200);
  lora.initialize();
  delay(1000); // cuts off Serial Monitor output w/o this
  pinMode(hall_sensor_pin, INPUT);
  pinMode(voltage_measurement_pin, INPUT);
  pinMode(solar_sensor_pin, INPUT);
  pinMode(water_level_pin, INPUT);
  attachInterrupt(hall_sensor_pin, its_raining_isr, RISING);

#ifdef LORA_SETUP_REQUIRED
  lora.one_time_setup();
#endif

  // Add the appropriate "set" method(s) here to change most of
  // the LoRa parameters, if desired. If you do, use the appropriate 
  // AT command to display the result of the change, to make sure it changed. 
  // EXAMPLE: lora->set_output_power(10);
  //          lora->send_and_reply("AT+CRFOP?");;

  delay(1000); // Serial.monitor needs a few seconds to get ready

    // Send the water level
    float water_volume = water_volume_sensor.reported_water_level();
    Serial.println("Reported_water_volume:" + (String)water_volume);
    lora.send_water_volume_data(water_volume);
    
    // Send the battery voltage
    float voltage = voltage_sensor.reported_voltage();
    Serial.println("Reported_voltage:" + (String)voltage);
    lora.send_voltage_data(voltage);
    delay(1000); // may not be necessary, but it can't hurt

    // Send the solar sensor level

    // Send the temp, humidity, and baro pressure

   // Go to deep sleep for 15 minutes, if it's not raining.
  delay(2000);
  if (rain_counter == 0) {
    lora.turn_off(); // to save battery power while asleep
    Serial.println("Going to sleep now");
    gettimeofday(&sleep_enter_time, NULL);
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
    esp_deep_sleep_start();
  }
  // if rain_counter > 0, it's raining, so we go into loop(), which will need some elapsedMillis timers
  // and some other things, so set them up here.
  // Get the system time and use it to calculate how long we were in deep sleep.
  // Doesn't matter that the ESP32 doesn't know the actual time; the only thing that
  // matters is that the RTC clock has SOME time, and that it keeps ticking during
  // deep sleep (which it does).
  periods_without_rain = 0;

  // BAS: I'm not sure I need the rest of the lines in setup().
  struct timeval now;
  gettimeofday(&now, NULL);
  time_in_deep_sleep_seconds = now.tv_sec - sleep_enter_time.tv_sec;
  // Set up a timer for 15 minutes minus time_in_deep_sleep_seconds, until going to sleep again (if it's done raining)
  data_send_timer = TIME_TO_SLEEP - time_in_deep_sleep_seconds;

} // setup()

void loop() { // entered only if it's raining; goes to sleep when it stops raining
  
  if (rain_data_submit_timer > RAIN_DATA_INTERVAL) { // time to send rain rate data
    if (rain_counter == last_rain_counter) { // it's not raining
      periods_without_rain++;
    }
    last_rain_counter = rain_counter;
    rain_counter = 0;
    rain_data_submit_timer = 0;
    float rain_rate = rain_sensor.reported_rain_rate(last_rain_counter);
    Serial.println("Reported_rain_rate:" + (String)rain_rate);
    lora.send_rain_rate(rain_rate);
  }

  if (data_submit_timer > TIME_TO_SLEEP) { // TIME_TO_SLEEP is also the non-rain data submit interval
    data_submit_timer = 0;
    // Send the water level
    float water_volume = water_volume_sensor.reported_water_level();
    Serial.println("Reported_water_volume:" + (String)water_volume);
    lora.send_water_volume_data(water_volume);
    
    // Send the battery voltage
    float voltage = voltage_sensor.reported_voltage();
    Serial.println("Reported_voltage:" + (String)voltage);
    lora.send_voltage_data(voltage);
    delay(1000); // may not be necessary, but it can't hurt

    // Send the solar sensor level

    // Send the temp, humidity, and baro pressure
  }

  if (periods_without_rain >= RAIN_SENSE_DURATION) { // it stopped raining, so reset everyting and go to sleep
    // BAS: go to sleep, with the sleep timer, and wake up at the next 15 minute interval for non-rain data
    rain_counter = 0;
    uint16_t seconds_to_sleep = TIME_TO_SLEEP - (uint16_t)(data_submit_timer / 1000); // remainder of the normal sleep time
    // BAS: need to somehow save seconds_to_sleep, in case it starts raining again before it's time to wake up
    lora.turn_off(); // to save battery power while asleep
    Serial.println("Going to sleep now");
    gettimeofday(&sleep_enter_time, NULL);
    esp_sleep_enable_timer_wakeup(seconds_to_sleep * uS_TO_S_FACTOR);
    esp_deep_sleep_start();
  }
}
