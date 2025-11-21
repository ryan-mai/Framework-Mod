#include <Arduino.h>

#include <ArduinoJson.h>
#include <HttpClient.h>
#include <WiFi.h>

#include <TimeLib.h>

#include <Wire.h>
#include <LiquidCrystal.h>

#include "secrets.h"

const char* apiKey = "if i commit my api key ill rotate it hopefully";
const char* city = "Tokyo,jp";

const char* server = "https://api.weatherapi.com/v1/current.json";
// Setup
// LiquidCrystal_I2C lcd(0x3F, 16, 2);

unsigned long lastUpdated = 0;
const long updateInterval = 600000; // 10 mins;
const long connectionDelay = 500;

void getWeather() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    WiFiClientSecure client;
    client.setInsecure();

    String url = String(server) + "?key=" + apiKey + "&q=" + city + "&aqi=no";
    
    if (http.begin(client, url)) {
      int httpCode = http.GET();
      if (httpCode > 0) {
        String data = http.getString();
        Serial.println(data);
      } else {
        Serial.printf("http.GET() failed, error %d\n", httpCode);
      }
      http.end();
    } else {
      Serial.println("Could not make http.GET() request");
    }
  } else {
    Serial.println("Could not connect to WiFi!");
  }
}

String nf(int num) {
  if (num < 10) return "0" + String(num);
  return String(num);
}

String formateDate() {
  return String(year()) + "-" + nf(month()) + "-" + nf(day()) + " " + nf(hour()) + ":" + nf(minute()) + ":" + nf(second());
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  while (!Serial);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(connectionDelay);
    Serial.println("Connecting to WiFi...");
  }

  Serial.println("Connected to the WiFi!");
  Serial.println("");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  // lcd.init();
  // lcd.backlight();
  // lcd.clear();
  // lcd.setCursor(0, 0);
}

void loop() {
  if (Serial.available()) {
    char t = Serial.read();
    if (t == 'T') {
      unsigned long timestamp = Serial.parseInt();
      setTime(timestamp);
    }
  }

  if (millis() - lastUpdated > updateInterval) {
    getWeather();
    lastUpdated = millis();
  }
}
