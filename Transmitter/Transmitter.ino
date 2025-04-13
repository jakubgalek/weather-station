#include <RH_ASK.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_BME280.h>
#include <LowPower.h>
#include <avr/power.h>   // Turn off unnecessary peripherals

#define ONE_WIRE_BUS 2       // DS18B20
#define PIN_TRANSMITER 11

RH_ASK driver(2000, 10, PIN_TRANSMITER, 5);
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
Adafruit_BME280 bme;  // I2C

void setup() {
  // Serial.begin(9600);
  power_usart0_disable();

  if (!driver.init()) {
    // Serial.println("RF 433 init failed!");
  }
  
  sensors.begin(); // DS18B20 init
  if (!bme.begin(0x76)) {
    // Serial.println("Could not find a valid BME280!");
  }
}

void loop() {
  // Request for temperature from DS18B20
  sensors.requestTemperatures();

  float temperature = sensors.getTempCByIndex(0);

  float humidity = bme.readHumidity();

  // Create message about sensors data
  String msg = String(temperature, 2) + "|" + String(humidity, 2);

  //Serial.print("Sending message: ");
  //Serial.println(msg);

  // Send RF message
  driver.send((uint8_t *)msg.c_str(), msg.length());
  driver.waitPacketSent();

  // Sleep for 8 seconds
  LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);

}
