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

#define PIN_BACKLIGHT_BUTTON D3

bool isBacklightOn = true;
bool buttonPressed = false;

LiquidCrystal_I2C lcd(0x27, 16, 2);

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

  pinMode(PIN_BACKLIGHT_BUTTON, INPUT_PULLUP);

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


  bool bmeStatus = bme.begin(0x76);

  server.on("/toggleBacklight", HTTP_GET, handleToggleBacklight);
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

  handleBacklightButton();

  if (millis() - lastReceiveTime > 30000) {
    wasReceived = false;
    receivedTemperature = 0;
    receivedHumidity = 0;
  }

  static unsigned long lastUpdateTime = 0;

  if (millis() - lastUpdateTime >= 2000) {
    lastUpdateTime = millis();
      
    if (bme.begin(0x76)) {

      float pressureOffset = 30;
      currentPressure = bme.readPressure() / 100.0 + pressureOffset;

      float temperatureOffset = -0.7;
      currentTemperature =  bme.readTemperature() + temperatureOffset;
      currentHumidity = bme.readHumidity();

      lcd.setCursor(0, 0);
      lcd.write((byte)1);
      lcd.print(currentTemperature, 0);
      lcd.write((byte)0);  // Show degree character
      lcd.print("C");

      if (currentPressure>=1000)
      lcd.setCursor(8, 0);
      else
      lcd.setCursor(9, 0);
      lcd.print(currentPressure, 0);
      lcd.print(" hPa");

    } 
    
    else {
      lcd.print("                ");
      lcd.setCursor(0, 0);
      lcd.print("Brak BME280!");
    }
    
      lcd.setCursor(0, 1);
      lcd.write((byte)1);
      if (wasReceived == true)
      {
        lcd.print(receivedTemperature, 0);
        lcd.write((byte)0);
        lcd.print("C");
      }
      else
      {
        lcd.write((byte)0);
        lcd.print("C");
        lcd.print("    ");
      }

      lcd.setCursor(10, 1);
      lcd.write((byte)2);
      if (wasReceived == false)
        lcd.print(currentHumidity, 1);
      else
      lcd.print(receivedHumidity, 1);
      lcd.print("%");

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

void handleBacklightButton() {
  int backlightButtonState = digitalRead(PIN_BACKLIGHT_BUTTON);

  // Check if the button has been pressed
  if (backlightButtonState == LOW && !buttonPressed) {
    buttonPressed = true; 

    isBacklightOn = !isBacklightOn;

    if (isBacklightOn) {
      lcd.backlight();
    } else {
      lcd.noBacklight();
    }
    delay(500);
  } else if (backlightButtonState == HIGH && buttonPressed) {
    buttonPressed = false; 
  }
}

void handleToggleBacklight() {
  if (isBacklightOn) {
    lcd.noBacklight();
  } else {
    lcd.backlight();
  }

  isBacklightOn = !isBacklightOn;  // Zaktualizuj stan podświetlenia
  server.send(200, "text/plain", "Backlight toggled");
}



void handleData() {
    String jsonData;

    String outsideTemperatureString = String(receivedTemperature, 1);
    String outsideHumidityString = String(receivedHumidity, 0);

    if (isnan(receivedTemperature)) {
        outsideTemperatureString = "\"NaN\"";
    }

    if (isnan(receivedHumidity)) {
        outsideHumidityString = "\"NaN\"";
    }

    // Utwórz JSON
    jsonData = "{\"temperature\": " + String(currentTemperature, 1) +
               ", \"humidity\": " + String(currentHumidity, 0) +
               ", \"pressure\": " + String(currentPressure, 0) +
               ", \"outside_temperature\": " + outsideTemperatureString +
               ", \"outside_humidity\": " + outsideHumidityString + "}";

    // Wyślij odpowiedź
    server.send(200, "application/json", jsonData);
}


void handleRoot() {
  String html = "<!DOCTYPE html>";
  html += "<html lang='pl'>";
  html += "<head>";
  html += "    <meta charset='UTF-8'>";
  html += "    <meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "    <title>Stacja pogodowa</title>";
  html += "    <style>";
  html += "        body {";
  html += "            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;";
  html += "            margin: 0;";
  html += "            padding: 0;";
  html += "            background-color: #292929;";
  html += "            color: #FFFFFF;";
  html += "        }";
  html += "";
  html += "        header {";
  html += "            background-color: #11864c;";
  html += "            color: #DCDCDC;";
  html += "            text-align: center;";
  html += "            padding: 0.1em;";
  html += "        }";
  html += "";
  html += "        h3 {";
  html += "            text-align: center;";
  html += "            margin: 2em 0 1.4em 0;";
  html += "        }";
  html += "";
  html += "        .data-group {";
  html += "            display: flex;";
  html += "            flex-wrap: wrap;";
  html += "            justify-content: center;";
  html += "            gap: 20px;";
  html += "            margin-bottom: 2em;";
  html += "        }";
  html += "";
  html += "        .data-container {";
  html += "            height: 6em;";
  html += "            border: 2px solid #0f8e58;";
  html += "            border-radius: 10px;";
  html += "            padding: 0;";
  html += "            flex: 0 1 150px;";
  html += "            max-width: 140px;";
  html += "            text-align: center;";
  html += "            background-color: #1f1f1f;";
  html += "            overflow: hidden;";
  html += "        }";
  html += "";
  html += "        .data-container p {";
  html += "            margin: 0;";
  html += "            padding: 0.4em;";
  html += "            font-weight: bold;";
  html += "            background-color: #0f8e58;";
  html += "            color: #FFFFFF;";
  html += "        }";
  html += "";
  html += "        .data-container span {";
  html += "            font-size: 24px;";
  html += "            padding: 0.5em;";
  html += "            padding-right: 0;";
  html += "            display: inline-block;";

  html += "        }";
  html += "";
  html += "        .data-container .unit {";
  html += "            font-size: 16px;";
  html += "            padding-left: 0.3em;";
  html += "            color: #CCCCCC;";
  html += "        }";
  html += "";
  html += "        button {";
  html += "            margin: 2em auto;";
  html += "            padding: 0.6em 1.2em;";
  html += "            font-size: 16px;";
  html += "            background-color: #121a1599;";
  html += "            color: #DCDCDC;";
  html += "            border-radius: 5px;";
  html += "            cursor: pointer;";
  html += "            border-color: #a9ff00e8;";
  html += "            display: block;";
  html += "            width: 160px;";
  html += "        }";
  html += "";
  html += "        button:hover {";
  html += "            background-color: #1b261f;";
  html += "        }";
  html += "";
  html += "        @media (max-width: 768px) {";
  html += "            .data-group {";
  html += "                flex-direction: row;";
  html += "                flex-wrap: wrap;";
  html += "                gap: 20px;";
  html += "            }";
  html += "";
  html += "            .data-container {";
  html += "                flex: 1 1 45%;";
  html += "                max-width: 150px;";
  html += "            }";
  html += "        }";
  html += "    </style>";
  html += "</head>";
  html += "<body>";
  html += "    <header>";
  html += "        <h1>Stacja pogodowa</h1>";
  html += "    </header>";
  html += "";
  html += "    <section>";
  html += "        <h3>W domu 🏠</h3>";
  html += "        <div class='data-group'>";
  html += "            <div class='data-container'>";
  html += "                <p>Temperatura</p>";
  html += "                <span id='temperature'>0.0</span><span class='unit'>°C</span>";
  html += "            </div>";
  html += "            <div class='data-container'>";
  html += "                <p>Wilgotność</p>";
  html += "                <span id='humidity'>0.0</span><span class='unit'>%</span>";
  html += "            </div>";
  html += "            <div class='data-container'>";
  html += "                <p>Ciśnienie</p>";
  html += "                <span id='pressure'>0.0</span><span class='unit'>hPa</span>";
  html += "            </div>";
  html += "        </div>";
  html += "    </section>";
  html += "";
  html += "    <section>";
  html += "        <h3>Na zewnątrz 🌳</h3>";
  html += "        <div class='data-group'>";
  html += "            <div class='data-container'>";
  html += "                <p>Temperatura</p>";
  html += "                <span id='outside_temperature'>0.0</span><span class='unit'>°C</span>";
  html += "            </div>";
  html += "            <div class='data-container'>";
  html += "                <p>Wilgotność</p>";
  html += "                <span id='outside_humidity'>0.0</span><span class='unit'>%</span>";
  html += "            </div>";
  html += "        </div>";
  html += "    </section>";
  html += "";
  html += "    <section>";
  html += "        <button onclick='toggleBacklight()'>🔆 Wyświetlacz</button>";
  html += "    </section>";
  html += "";
  html += "    <script>";
  html += "        function updateData() {";
  html += "            var xhttp = new XMLHttpRequest();";
  html += "            xhttp.onreadystatechange = function () {";
  html += "                if (this.readyState == 4 && this.status == 200) {";
  html += "                    var data = JSON.parse(this.responseText);";
  html += "";
  html += "                    document.getElementById('temperature').innerHTML = data.temperature;";
  html += "                    document.getElementById('humidity').innerHTML = data.humidity;";
  html += "                    document.getElementById('pressure').innerHTML = data.pressure;";
  html += "                    document.getElementById('outside_temperature').innerHTML = data.outside_temperature;";
  html += "                    document.getElementById('outside_humidity').innerHTML = data.outside_humidity;";
  html += "                }";
  html += "            };";
  html += "            xhttp.open('GET', '/data', true);";
  html += "            xhttp.send();";
  html += "        }";
  html += "";
html += "        function toggleBacklight() {";
html += "            var xhttp = new XMLHttpRequest();";
html += "            xhttp.open('GET', '/toggleBacklight', true);";
html += "            xhttp.send();";
html += "        };";
  html += "";
  html += "        setInterval(updateData, 2000);";
  html += "    </script>";
  html += "</body>";
  html += "</html>";

  server.send(200, "text/html", html);
}

