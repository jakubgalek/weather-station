#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiClient.h>
#include <ArduinoOTA.h>
#include <RH_ASK.h>

#define PIN_RECEIVER D5

LiquidCrystal_I2C lcd(0x27, 20, 4);

byte degreeIcon[8] = {
  B00110,
  B01001,
  B01001,
  B00110,
  B00000,
  B00000,
  B00000,
  B00000
};
byte thermometerIcon[8] = {
  B00100,
  B01010,
  B01010,
  B01110,
  B01110,
  B11111,
  B11111,
  B01110
};
byte humidityIcon[8] = {
  B00100,
  B00100,
  B01010,
  B01010,
  B10001,
  B10001,
  B10001,
  B01110
};

Adafruit_BME280 bme;

const char *ssid = "";
const char *password = "";

ESP8266WebServer server(80);

float currentPressure;
float currentTemperature;
float currentHumidity;

RH_ASK driver(2000, PIN_RECEIVER, 4, 5);

float receivedTemperature;
float receivedHumidity;   

bool wasReceived = false;
unsigned long lastReceiveTime = 0;


void setup() {
  Serial.begin(115200);

  lcd.init();
  lcd.backlight();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  // Setup ArduinoOTA
  ArduinoOTA.onStart([]() {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Aktualizacja");
  });
  ArduinoOTA.onEnd([]() {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Ukonczono");
    lcd.setCursor(0, 1);
    lcd.print("aktualizacje!");
    delay(1000);
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    lcd.setCursor(0, 1);
    lcd.print("Postep: ");
    lcd.print((progress / (total / 100)));
    lcd.print("%");
  });
  ArduinoOTA.onError([](ota_error_t error) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("OTA Error");
    if (error == OTA_AUTH_ERROR) lcd.print("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) lcd.print("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) lcd.print("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) lcd.print("Receive Failed");
    else if (error == OTA_END_ERROR) lcd.print("End Failed");
    delay(1000);
  });

  // Start ArduinoOTA
  ArduinoOTA.begin();

  server.on("/", HTTP_GET, handleRoot);
  server.on("/data", HTTP_GET, handleData);

  server.begin();

  MDNS.begin("stacja");
  MDNS.addService("http", "tcp", 80);

  lcd.createChar(0, degreeIcon); 
  lcd.createChar(1, thermometerIcon);  
  lcd.createChar(2, humidityIcon);

    if (!driver.init())
    Serial.println("init failed");
}

void loop() {

  // Handle OTA updates
  ArduinoOTA.handle();

  if (WiFi.status() == WL_CONNECTED) {
    server.handleClient();
    MDNS.update();
  }

  if (millis() - lastReceiveTime > 10000) {
    wasReceived = false;
    receivedTemperature = 0;
    receivedHumidity = 0;
  }

  static unsigned long lastUpdateTime = 0;

  if (millis() - lastUpdateTime >= 2000) {
    lastUpdateTime = millis();

    if (bme.begin(0x76)) {
      
      float pressureOffset = 5;
      currentPressure = bme.readPressure() / 100.0 + pressureOffset;

      float temperatureOffset = -0.7;
      currentTemperature =  bme.readTemperature() + temperatureOffset;
      currentHumidity = bme.readHumidity();

      lcd.setCursor(0, 0);
      lcd.write((byte)1);
      lcd.print(currentTemperature, 1);
      lcd.write((byte)0);  // Show degree character
      lcd.print("C");

      if (currentPressure>=1000)
      lcd.setCursor(8, 0);
      else
      lcd.setCursor(9, 0);
      lcd.print(currentPressure, 0);
      lcd.print(" hPa");


      lcd.setCursor(0, 1);
      lcd.write((byte)1);
      if (wasReceived == true)
        lcd.print(receivedTemperature, 1);
      lcd.write((byte)0);
      lcd.print("C");


      lcd.setCursor(10, 1);
      lcd.write((byte)2);
      if (wasReceived == false)
        lcd.print(currentHumidity, 1);
      else
      lcd.print(receivedHumidity, 1);
      lcd.print("%");

    } else {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Brak czujnika!");
    }
  }
  uint8_t buf[100];
  uint8_t buflen = sizeof(buf);

  if (driver.recv(buf, &buflen)) {
    
    lastReceiveTime = millis();
    wasReceived = true;
    
    Serial.print("Received: ");
    for (int i = 0; i < buflen; i++) {
      Serial.print((char)buf[i]);
    }
    
    // Parsing received message
    String receivedData = String((char*)buf);
    int separatorIndex = receivedData.indexOf('|');
    
    if (separatorIndex != -1) {
      receivedTemperature = receivedData.substring(0, separatorIndex).toFloat();
      receivedHumidity = receivedData.substring(separatorIndex + 1).toFloat();
      }
  }
}

void handleData() {
    String jsonData = "{\"temperature\": " + String(currentTemperature, 1) + ", \"humidity\": " + String(currentHumidity, 0) + ", \"pressure\": " + String(currentPressure, 0) + 
    ", \"outside_temperature\": " + String(receivedTemperature, 1) + ", \"outside_humidity\": " +  String(receivedHumidity, 0) +"}";
    server.send(200, "application/json", jsonData);
}

void handleRoot() {
String html = "<!DOCTYPE html>";
html += "<html lang='pl'>";
html += "<head>";
html += "    <meta charset='UTF-8'>";
html += "    <meta name='viewport' content='width=device-width, initial-scale=1.0'>";
html += "    <style>";
html += "        body {";
html += "            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;";
html += "            margin: 0;";
html += "            padding: 0;";
html += "            background-color: #292929;";
html += "            color: #FFFFFF;";
html += "        }";
html += "        header {";
html += "            background-color: #11864c;";
html += "            color: #DCDCDC;";
html += "            text-align: center;";
html += "            padding: 0.1em;";
html += "        }";
html += "        section {";
html += "            text-align: center;";
html += "            padding: 0em;";
html += "        }";
html += "        p {";
html += "            margin: 0;";
html += "            padding: 0.5em;";
html += "        }";
html += "        #pressure-container, #temperature-container, #humidity-container {";
html += "            border: 2px solid #0f8e58;";
html += "            border-radius: 10px;";
html += "            margin: 10px;";
html += "            padding: 10px;";
html += "            display: inline-block;"; 
html += "            width: calc(8% - 13px);";
html += "        }";
html += "        #pressure, #temperature, #humidity, #outside_temperature, #outside_humidity {";
html += "            display: none;";
html += "        }";
html += "        @media (max-width: 768px) {";
html += "            #pressure-container, #temperature-container, #humidity-container {";
html += "                width: calc(33.33% - 8px);";
html += "            }";
html += "        }";
html += "    </style>";
html += "</head>";
html += "<body>";
html += "    <header>";
html += "        <h1>Stacja pogodowa</h1>";
html += "    </header>";
html += "    <section>";
html += "        <h3>W domu</h3>";
html += "        <div id='temperature-container'>";
html += "            <p>Temperatura: <span id='temperature' style='font-size: 24px;'>0.0</span> °C</p>";
html += "        </div>";
html += "        <div id='humidity-container'>";
html += "            <p>Wilgotność: <span id='humidity' style='font-size: 24px;'>0.0</span> %</p>";
html += "        </div>";
html += "        <div id='pressure-container'>";
html += "            <p>Ciśnienie: <span id='pressure' style='font-size: 24px;'>0.0</span> hPa</p>";
html += "        </div>";
html += "    </section>";
html += "    <section>";
html += "        <h3>Na zewnątrz</h3>";
html += "        <div id='temperature-container'>";
html += "            <p>Temperatura: <span id='outside_temperature' style='font-size: 24px;'>0.0</span> °C</p>";
html += "        </div>";
html += "        <div id='humidity-container'>";
html += "            <p>Wilgotność: <span id='outside_humidity' style='font-size: 24px;'>0.0</span> %</p>";
html += "        </div>";
html += "    </section>";
html += "    <script>";
html += "        function updateData() {";
html += "            var xhttp = new XMLHttpRequest();";
html += "            xhttp.onreadystatechange = function() {";
html += "                if (this.readyState == 4 && this.status == 200) {";
html += "                    var data = JSON.parse(this.responseText);";

html += "                    document.getElementById('temperature').innerHTML = data.temperature;";
html += "                    document.getElementById('humidity').innerHTML = data.humidity;";
html += "                    document.getElementById('pressure').innerHTML = data.pressure;";
html += "                    document.getElementById('temperature').style.display = 'inline-block';";
html += "                    document.getElementById('humidity').style.display = 'inline-block';";
html += "                    document.getElementById('pressure').style.display = 'inline-block';";

html += "                    document.getElementById('outside_temperature').innerHTML = data.outside_temperature;";
html += "                    document.getElementById('outside_humidity').innerHTML = data.outside_humidity;";
html += "                    document.getElementById('outside_temperature').style.display = 'inline-block';";
html += "                    document.getElementById('outside_humidity').style.display = 'inline-block';";
html += "                }";
html += "            };";
html += "            xhttp.open('GET', '/data', true);";
html += "            xhttp.send();";
html += "        }";
html += "        setInterval(updateData, 2000);";
html += "    </script>";
html += "</body>";
html += "</html>";


  server.send(200, "text/html", html);
}