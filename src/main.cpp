// Use board "ESP32 DEVKIT V1"
/* 
This is the version of the LoRa Transmitter that transmits data
from the Weather Station.
Data sent: Rain rate, Water level (in the canal), battery voltage,
sun intensity, temperature, humidity, barometric pressure.
*/

#include <Arduino.h>
#include "reyax_lora.h"
#include "analog_reader.h"
#include "water_volume_sensor.h"
#include "rain_rate_sensor.h"
#include "bme280.h"
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

uint8_t hall_sensor_pin = 32; // goes LOW when sensing the magnet
uint8_t voltage_measurement_pin = 33;
uint8_t solar_sensor_pin = 34;
uint8_t water_level_pin = 35;
// var to keep track of how many times the tipping bucket has dumped since the rain started
// I don't think it needs to be in RTC_DATA_ATTR, but it might so it can be incremented by the ISR
// when the Hall Sensor pin wakes up the ESP32, so the ISR can increment it.
RTC_DATA_ATTR uint16_t rain_counter = 0; //BAS: after it works, try to remove the RTC_DATA_ATTR and see if it still works.
elapsedMillis NRD_timer_ms = 0; // NRD = Non-Rain Data
elapsedMillis rain_data_timer_ms = 0;

ReyaxLoRa lora(0);
VoltageSensor voltage_sensor(voltage_measurement_pin);
WaterVolumeSensor water_volume_sensor(water_level_pin);
RainRateSensor rain_sensor;
BME280Sensor bme280;

/* Variable to store the time at which deep sleep is entered. Used to
   calculate the length of time in deep sleep, if the wakeup happens
   because of the Hall Sensor interrupt (it started to rain).
*/
RTC_DATA_ATTR struct timeval time_put_to_sleep;

/* Variable to store the remaining amount of time until the next
   NRD s/b sent.
*/
RTC_DATA_ATTR uint16_t seconds_to_next_NRD_send;

/* To record each dumping of the tipping bucket. BAS: will this execute the first time,
when the interrupt is the thing that wakes up the ESP32 from deep sleep?
*/ 
void IRAM_ATTR its_raining_isr() {
  rain_counter++;
}

void setup() {  
  Serial.begin(115200);
  lora.initialize();
  delay(1000); // cuts off Serial Monitor output w/o this
  pinMode(hall_sensor_pin, INPUT);
  pinMode(voltage_measurement_pin, INPUT);
  pinMode(solar_sensor_pin, INPUT);
  pinMode(water_level_pin, INPUT);
  attachInterrupt(hall_sensor_pin, its_raining_isr, FALLING);

#ifdef LORA_SETUP_REQUIRED
  lora.one_time_setup();
#endif

  // Add the appropriate "set" method(s) here to change most of
  // the LoRa parameters, if desired. If you do, use the appropriate 
  // AT command to display the result of the change, to make sure it changed. 
  // EXAMPLE: lora->set_output_power(10);
  //          lora->send_and_reply("AT+CRFOP?");

  delay(1000); // Serial.monitor needs a few seconds to get ready

  // Determine the reason for the wakeup here. If not the interrupt, send all non-rain data,
  // then see if it might have started raining since waking up (very unlikely, but possible),
  // and if not, go back to sleep. If the interrupt, or if it's raining by the time the non-rain
  // data is sent, don't go to sleep. Instead, drop into loop() to monitor the rain, until it stops. 
  if (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_EXT0) { // TIMER wakeup, or first run after a reboot

    seconds_to_next_NRD_send = NRD_INTERVAL; // reset to the normal sleep period
    NRD_timer_ms = 0; // start the timer, in case it starts to rain before we go to sleep
    
    // Send the water level
    float water_volume = water_volume_sensor.reported_water_level();
    Serial.println("Reported_water_volume:" + (String)water_volume);
    lora.send_water_volume_data(water_volume);
    
    // Send the battery voltage
    float voltage = voltage_sensor.reported_voltage();
    Serial.println("Reported_voltage:" + (String)voltage);
    lora.send_voltage_data(voltage);

    // BAS: Send the solar sensor level

    // Send the temp, humidity, and baro pressure
    String temperature = String(bme280.reported_temp(), 0);
    Serial.println("Reported_temp:" + temperature);
    lora.generate_and_send_payload("Temp", temperature, 0, 0, 0);

    String pressure = String(bme280.reported_baro_press(), 0);
    Serial.println("Reported_pressure:" + pressure);
    lora.generate_and_send_payload("Press", pressure, 0, 0, 0);

    String humidity = String(bme280.reported_humidity(), 0);
    Serial.println("Reported_humidity:" + humidity);
    lora.generate_and_send_payload("Humid", humidity, 0, 0, 0);

    delay(2000);
    if (rain_counter == 0) { // it's not raining
      // lora.turn_off(); // to save battery power while asleep BAS: need to modify PCB to do this
      Serial.println("Going to sleep now");
      // save current time in case we wakeup from an interrupt
      gettimeofday(&time_put_to_sleep, NULL);
      // calculate remaining time until a normal timer wakeup
      uint16_t seconds_to_sleep = seconds_to_next_NRD_send - (uint16_t)(NRD_timer_ms / 1000);
      // configure deep sleep, and go to sleep
      esp_sleep_enable_timer_wakeup(seconds_to_sleep * uS_TO_S_FACTOR);
      esp_sleep_enable_ext0_wakeup(GPIO_NUM_32, 0); // BAS: make sure that each swipe of the magnet will produce a 1 AND a 0.
      esp_deep_sleep_start();
    }
  }
  // Waking up because it's raining, so determine how long we've been asleep, 
  // and how much time is remaining until the NRD needs to be sent
  else {
    struct timeval now;
    gettimeofday(&now, NULL);
    uint32_t seconds_asleep = now.tv_sec - time_put_to_sleep.tv_sec;
    seconds_to_next_NRD_send = seconds_to_next_NRD_send - seconds_asleep;
    // start the timer to know when it's time to send the next NRD
    NRD_timer_ms = 0;
  }
} // setup()

void loop() { // entered only if it's raining; goes to sleep when it stops raining
  
  static uint16_t last_rain_counter = 0;
  static uint8_t periods_without_rain = 0;

  if (rain_data_timer_ms > RAIN_DATA_INTERVAL * 1000) { // it's time to send rain rate data
    if (last_rain_counter == rain_counter) { // its not raining 
       periods_without_rain++;
    }
    else { // it's raining
       periods_without_rain = 0;
    }
    float rain_rate = rain_sensor.reported_rain_rate(rain_counter - last_rain_counter, rain_data_timer_ms);
    rain_data_timer_ms = 0;
    last_rain_counter = rain_counter;
    Serial.println("Reported_rain_rate:" + (String)rain_rate);
    lora.send_rain_rate(rain_rate);
  }

  if (NRD_timer_ms > seconds_to_next_NRD_send * 1000) { // it's time to send NRD
    // reset the interval and the timer
    NRD_timer_ms = 0;
    seconds_to_next_NRD_send = NRD_INTERVAL;

    // Send the water level
    float water_volume = water_volume_sensor.reported_water_level();
    Serial.println("Reported_water_volume:" + (String)water_volume);
    lora.send_water_volume_data(water_volume);
    
    // Send the battery voltage
    float voltage = voltage_sensor.reported_voltage();
    Serial.println("Reported_voltage:" + (String)voltage);
    lora.send_voltage_data(voltage);
    delay(1000); // may not be necessary, but it can't hurt

    // BAS: Send the solar sensor level

    String temperature = String(bme280.reported_temp(), 0);
    Serial.println("Reported_temp:" + temperature);
    lora.generate_and_send_payload("Temp", temperature, 0, 0, 0);

    String pressure = String(bme280.reported_baro_press(), 0);
    Serial.println("Reported_pressure:" + pressure);
    lora.generate_and_send_payload("Press", pressure, 0, 0, 0);

    String humidity = String(bme280.reported_humidity(), 0);
    Serial.println("Reported_humidity:" + humidity);
    lora.generate_and_send_payload("Humid", humidity, 0, 0, 0);
  }

  if (periods_without_rain >= RAIN_SENSE_DURATION) { // it stopped raining, so reset everyting and go to sleep
    rain_counter = 0;
    // lora.turn_off(); // to save battery power while asleep BAS: need to modify PCB to do this
    Serial.println("Going to sleep now");
    // save current time in case we wakeup from an interrupt
    gettimeofday(&time_put_to_sleep, NULL);
    // calculate remaining time until a normal timer wakeup
    seconds_to_next_NRD_send = seconds_to_next_NRD_send - (uint16_t)(NRD_timer_ms / 1000); // remainder of the normal sleep time
    // configure deep sleep, and go to sleep
    esp_sleep_enable_timer_wakeup(seconds_to_next_NRD_send * uS_TO_S_FACTOR);
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_32, 0); // BAS: make sure that each swipe of the magnet will produce a 1 AND a 0.
    esp_deep_sleep_start();
  }
  delay(5000);
}
