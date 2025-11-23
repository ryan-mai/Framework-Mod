#include <Arduino.h>

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>

// #include <TimeLib.h>

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#include <Keypad.h>

#include "secrets.h"

const char* city = "Toronto";

#define ROW_NUM 4
#define COLUMN_NUM 4

char keys[ROW_NUM][COLUMN_NUM] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'},
};

byte pin_rows[ROW_NUM] = {23, 19, 18, 5};
byte pin_column[COLUMN_NUM] = {17, 16, 4, 13};

Keypad keypad = Keypad( makeKeymap(keys), pin_rows, pin_column, ROW_NUM, COLUMN_NUM);

// Setup
LiquidCrystal_I2C LCD(0x3F, 16, 2);

unsigned long lastUpdated = 0;
const long updateInterval = 600000; // 10 mins;
const long busInterval = 30067; // 30.67 secs;
const long connectionDelay = 500;

void getBusRoute() {
  char key;
  String keyStr = "";
  String prevKeyStr = "";
  while (true) {
    char key = keypad.getKey();

    if (key != NO_KEY) {
      if (key == 'D') {
        break;
      } else if (key = 'A') {
        if (!keyStr.isEmpty()) {
          keyStr.remove(keyStr.length() - 1);
        }
      } else {
        keyStr += key;
      }
      if (keyStr != prevKeyStr) {
        prevKeyStr = keyStr;
        LCD.clear();
        LCD.setCursor(0, 1);
        LCD.print(keyStr);
      }
    }
  }
  LCD.clear();
  LCD.setCursor(0, 1);
  LCD.print(keyStr);
}

void getBusStops(int route) {

}


void getLocation() {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClientSecure client;
    client.setInsecure();
    int networks = WiFi.scanNetworks();
    JsonDocument doc;
    JsonArray wifiArray = doc["wifiAccessPoints"].to<JsonArray>();
    for (int i = 0; i < networks; i++) {
      JsonObject ap = wifiArray.add<JsonObject>();
      ap["macAddress"] = WiFi.BSSIDstr(i);
      ap["signalStrength"] = WiFi.RSSI(i);
      ap["signalToNoiseRatio"] = 1;
    }

    String jsonStr;
    serializeJson(doc, jsonStr);
    Serial.println();

    HTTPClient http;
    
    String url = "https://www.googleapis.com/geolocation/v1/geolocate?key=" + String(googleApiKey);
    Serial.println("Using the geolocation api: ");
    Serial.println(url);
    if (http.begin(client, url)) {
      http.addHeader("Content-Type", "application/json");
      int httpCode = http.POST(jsonStr);

      if (httpCode > 0) {
        String payload = http.getString();
        Serial.println(payload);

        JsonDocument doc;

        DeserializationError error = deserializeJson(doc, payload);
        if (error) {
          Serial.print(F("deserializeJson() failed"));
          Serial.println(error.c_str());
          return;
        }

        float lat = doc["location"]["lat"];
        float lng = doc["location"]["lng"];
      }
    }
  }
}

void getBus() {
  Serial.println("Accessing the train api");
  if (WiFi.status() == WL_CONNECTED) {
    String url = "https://retro.umoiq.com/service/publicJSONFeed?command=predictions&a=ttc&stopId=230";

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;

    Serial.println("Using the train url");
    Serial.println(url);
    if (http.begin(client, url)) {
      int httpCode = http.GET();
      if (httpCode > 0) {
        String payload = http.getString();
        Serial.println("Response: ");
        Serial.println(payload);

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload);
        if (error) {
          Serial.print(F("deserializeJson() failed"));
          Serial.print(error.c_str());
          return;
        }

        JsonArray items = doc["predictions"].as<JsonArray>();
        JsonObject routeObj;
        for (JsonObject obj : items) {
          if (obj["direction"].is<JsonObject>()) {
            routeObj = obj;
            break;
          }
        }
        if (!routeObj) {
          Serial.println("No directions found");
          LCD.clear();
          LCD.print("No Directions!");
          return;
        }

        JsonVariant predictionsObj = routeObj["direction"]["prediction"];
        JsonArray predictions;
        if (predictionsObj.is<JsonArray>()) {
          predictions =  predictionsObj.as<JsonArray>();
        } else {
          predictions = JsonArray();
        }
        
        if (predictions.size() == 0) {
          LCD.clear();
          LCD.print("No Bus Data!");
          return;
        }

        float time0 = predictions[0]["seconds"];
        int total_seconds = static_cast<int>(std::round(time0));
        int seconds = total_seconds % 60;
        int minutes = total_seconds / 60;
        char buf[17];
        snprintf(buf, sizeof(buf), "%d:%02d", minutes, seconds);
        LCD.setCursor(0, 0);
        LCD.printf(buf);

        if (predictions.size() > 1) {
          float time1 = predictions[1]["seconds"];
          int total_seconds_next = static_cast<int>(std::round(time1));
          int seconds_next = total_seconds_next % 60;
          int minutes_next = total_seconds_next / 60;
        
          LCD.setCursor(0, 1);
          LCD.printf("%d:%02d", minutes_next, seconds_next);
        } else {
            LCD.setCursor(0, 1);
            LCD.print("No next bus");
        }
      } else {
        Serial.printf("http.GET() failed, error %d\n", httpCode);
        LCD.clear();
        LCD.setCursor(0, 0);
        LCD.print("HTTP Error");
      }
      http.end();
    } else {
      Serial.println("Could not make HTTP request");
      LCD.clear();
      LCD.setCursor(0, 0);
      LCD.print("HTTP Request Failed");
    }
  } else {
    Serial.println("WiFi not connected");
    LCD.clear();
    LCD.setCursor(0, 0);
    LCD.print("WiFi Error");
  }
}

void getWeather() {
  Serial.println("Accessing the weather api");
  if (WiFi.status() == WL_CONNECTED) {
    String url = "https://api.weatherapi.com/v1/current.json?key=" + String(weatherApiKey) + "&q=" + city + "&aqi=no";;

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

        DeserializationError error = deserializeJson(doc, payload);
        if (error) {
          Serial.print(F("deserializeJson() failed"));
          Serial.println(error.c_str());
          return;
        }
        const char* region = doc["location"]["region"];
        float temp_c = doc["current"]["temp_c"];
        float feelslike_c = doc["current"]["feelslike_c"];
        const char* condition = doc["current"]["condition"]["text"];
        float wind_kph = doc["current"]["wind_kph"];
        LCD.clear();
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
  LCD.print(c[counter]);
  counter = (counter + 1) % 3;
  if (counter == strlen(c)) {
    counter = 0;
  }
}

void setup() {
  // put your setup code here, to run once:
  Wire.begin(21, 22);
  Wire.setClock(100000);
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
  getBusRoute();
  // getLocation();
  // getBus();
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

  if (millis() - lastUpdated > busInterval) {
    getBus();
    lastUpdated = millis();
  }
}
