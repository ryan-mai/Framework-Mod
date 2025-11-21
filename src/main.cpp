#include <Arduino.h>

#include <ArduinoJson.h>
#include <HttpClient.h>
#include <WiFi.h>

#include <TimeLib.h>

#include <Wire.h>
#include <LiquidCrystal.h>

const char* ssid = "mimimimi";
const char* password = "nopasswordfr jk";

const char* apiKey = "if i commit my api key ill rotate it hopefully";
const char* city = "Tokyo,jp";

const char* server = "https://api.weatherapi.com/v1/current.json";
// Setup
// LiquidCrystal_I2C lcd(0x3F, 16, 2);

unsigned long lastUpdated = 0;
const long updateInterval = 60000; // 10 mins;
const long connectionDelay = 500;
void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  while (!Serial);

  Wifi.begin(ssid, password);

  while (Wifi.status() != WL_CONNECTED) {
    delay(connectionDelay);
    Serial.println("Connecting to WiFi...")
  }

  Serial.println("Connected to the WiFi!");
  Serial.println("");
  Serial.println("IP address: ");
  Serial.println(Wifi.localIP());
  // lcd.init();
  // lcd.backlight();
  // lcd.clear();
  // lcd.setCursor(0, 0);
}

void loop() {
  // put your main code here, to run repeatedly:
  if (Serial.available()) {
    char t = Serial.read();
    if (t == 'T') {
      unsigned long timestamp = Serial.parseInt();
      setTime(timestamp);

      Serial.print("Time set: ");
      Serial.println(formatDate());
    }
  }

  if (millis() - lastUpdated > 1000) {
    lastPrint = millis();
    Serial.print("-----")
    Serial.println(formatDate());
    getWeather();
  }

}

void getWeather() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    String url = String(server) + "?key=" + apiKey + "&q=" + city + "&aqi=no"
    if (http.begin(url)) {
      if (http.GET() > 0) {
        String data = http.getString();
      }
      http.end();
    }
  }
}

String formateDate() {
  return String(year()) + "-" + nf(month(), 2) + "-" + nf(day(), 2) + " " + nf(hour(), 2) + ":" + (minute(), 2) + ":" + (second(), 2); 
}
