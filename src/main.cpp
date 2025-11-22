#include <Arduino.h>

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>

// #include <TimeLib.h>

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#include "secrets.h"
const char* city = "Tokyo";

// Setup
LiquidCrystal_I2C LCD(0x3F, 16, 2);

unsigned long lastUpdated = 0;
const long updateInterval = 600000; // 10 mins;
const long connectionDelay = 500;

void getWeather() {
  Serial.println("Accessing the weather api");;
  if (WiFi.status() == WL_CONNECTED) {
    String url = "https://api.weatherapi.com/v1/current.json?key=" + String(apiKey) + "&q=" + city + "&aqi=no";;

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;

    Serial.println("Using the weather url: ");
    Serial.println(url);
    if (http.begin(client, url)) {
      int httpCode = http.GET();

      if (httpCode > 0) {
        String payload = http.getString();
        Serial.println(payload);

        JsonDocument doc;

        DeserializationError error = deserializeJson(doc, client);
        if (error) {
          Serial.print(F("deseriializeJson() failed"));
          Serial.println(error.c_str());
          return;
        }
        const char* region = doc["location"]["region"];
        float temp_c = doc["current"]["temp_c"];
        float feelslike_c = doc["current"]["feelslike_c"];
        const char* condition = doc["current"]["condition"]["text"];
        float wind_kph = doc["current"]["wind_kph"];

        LCD.setCursor(0, 0);
        LCD.printf("%.13s", region);

        LCD.setCursor(14, 1);
        LCD.printf("%.0fC", temp_c);

        LCD.setCursor(0, 1);
        LCD.printf("%.14s", condition);

        Serial.printf("%s, | %.1f°C (feels %.1f°C) | %s | Wind %.1f\n",
                      region, temp_c, feelslike_c, condition, wind_kph
        );
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

// String formateDate() {
//   return String(year()) + "-" + nf(month()) + "-" + nf(day()) + " " + nf(hour()) + ":" + nf(minute()) + ":" + nf(second());
// }

void spinner() {
  static int8_t counter = 0;
  const char* c = "\xa1\xa5\xdb";
  LCD.setCursor(15, 1);
  LCD.print(c[counter + 1]);
  if (counter == strlen(c)) {
    counter = 0;
  }
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  while (!Serial);

  WiFi.begin(ssid, password);

  LCD.init();
  LCD.backlight();
  LCD.setCursor(0, 0);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(connectionDelay);
    Serial.println("Connecting to WiFi...");
    spinner();
  }

  Serial.println("");
  Serial.println("Connected to the WiFi!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  LCD.clear();
  LCD.setCursor(0, 0);
  LCD.println("Online");
  LCD.setCursor(0, 1);

  getWeather();
  lastUpdated = millis();
}

void loop() {
  // if (Serial.available()) {
  //   char t = Serial.read();
  //   if (t == 'T') {
  //     unsigned long timestamp = Serial.parseInt();
  //     setTime(timestamp);
  //   }
  // }

  if (millis() - lastUpdated > updateInterval) {
    getWeather();
    lastUpdated = millis();
  }
}
