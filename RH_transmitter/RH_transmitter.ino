#include <RH_ASK.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <DHT.h>

#define ONE_WIRE_BUS 2       // DS18B20
#define DHT_PIN 3            // DHT22

#define PIN_TRANSMITER 11

RH_ASK driver(2000, 10, PIN_TRANSMITER, 5);

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

#define DHTTYPE DHT22

DHT dht(DHT_PIN, DHTTYPE);


void setup() {
  Serial.begin(9600);
  if (!driver.init())
  Serial.println("init failed");
  sensors.begin();   // DS18B20 init
  dht.begin();
}

void loop() {

  // Request for temperatures
  sensors.requestTemperatures();         
  float temperature = sensors.getTempCByIndex(0);  // Temperature reading (in Celsius)

  float humidity = dht.readHumidity();

  if (isnan(humidity)) 
  return;

  // Create message about sesors data
  String msg = String(temperature) + "|" + String(humidity);

  Serial.print("Sending message: ");
  Serial.println(msg);

  // Send RF message
  driver.send((uint8_t *)msg.c_str(), msg.length());
  driver.waitPacketSent();

  delay(2000);  // Wait 2 seconds before sending next data
}
