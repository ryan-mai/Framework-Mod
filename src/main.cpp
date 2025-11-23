#include <Arduino.h>

#include <math.h>

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

bool getLocation(float* latOut, float* lngOut) {
  *latOut = 0.0f;
  *lngOut = 0.0f;
  
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
        http.end();

        JsonDocument doc;

        DeserializationError error = deserializeJson(doc, payload);
        if (error) {
          Serial.print(F("deserializeJson() failed"));
          Serial.println(error.c_str());
          return false;
        }

        *latOut = doc["location"]["lat"];
        *lngOut = doc["location"]["lng"];
        float accuracy = doc["accuracy"];
        Serial.printf("Location: %.7f, %.7f (accuracy: %.0f m)\n", *latOut,*lngOut, accuracy);
        return true;
      }
      *latOut = 0.0;
      *lngOut = 0.0;
    }
  }
  return false;
}

void getBus(String stopId) {
  Serial.println("Accessing the train api");
  if (WiFi.status() == WL_CONNECTED) {
    String url = "https://retro.umoiq.com/service/publicJSONFeed?command=predictions&a=ttc&stopId=" + stopId;

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

        JsonObject predictionsObj = doc["predictions"].as<JsonObject>();
        if (predictionsObj.isNull() || predictionsObj.size() == 0) {
          Serial.println("No prediction data");
          LCD.clear();
          LCD.print("No Bus Data!");
        }

        JsonObject routeObj = predictionsObj["direction"].as<JsonObject>();
        if (routeObj.isNull()) {
          Serial.println("No directions found");
          LCD.clear();
          LCD.print("No Directions!");
          return;
        }

        JsonVariant predVar = routeObj["prediction"];
        JsonArray predictions;

        if (predVar.is<JsonArray>()) {
          predictions = predVar.as<JsonArray>();
        } else if (predVar.is<JsonObject>()) {
          JsonDocument tempDoc;
          tempDoc.clear();
          predictions = tempDoc.to<JsonArray>();
          predictions.add(predVar);
        } else {
          LCD.clear();
          LCD.print("No upcoming!");
          return;
        }
        if (predictions.size() == 0) {
          LCD.clear();
          LCD.print("No upcoming!");
          return;
        }

        float time0 = atof( predictions[0]["seconds"] | "9999");
        int total_seconds = static_cast<int>(std::round(time0));
        int seconds = total_seconds % 60;
        int minutes = total_seconds / 60;

        char buf[17];
        snprintf(buf, sizeof(buf), "%d:%02d", minutes, seconds);
        LCD.setCursor(0, 0);
        LCD.printf(buf);

        if (predictions.size() > 1) {
          float time1 = atof(predictions[1]["seconds"] | "9999");
          int total_seconds_next = static_cast<int>(std::round(time1));
          int seconds_next = total_seconds_next % 60;
          int minutes_next = total_seconds_next / 60;
        
          LCD.setCursor(0, 1);
          LCD.printf("%d:%02d", minutes_next, seconds_next);
        } else {
            LCD.setCursor(0, 1);
            LCD.print("--:--");
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

String getBusNum() {
  char key;
  String keyStr = "";
  String prevKeyStr = "";
  while (true) {
    char key = keypad.getKey();

    if (key != NO_KEY) {
      if (key >= '0' && key <= '9') {
        keyStr += key;
      } else if (key == '*') {
        if (!keyStr.isEmpty()) {
          keyStr.remove(keyStr.length() - 1);
        }
      } else if (key == 'A' && !keyStr.isEmpty()) { keyStr += 'N'; break; }
      else if (key == 'B' && !keyStr.isEmpty()) { keyStr += 'E'; break; }
      else if (key == 'C' && !keyStr.isEmpty()) { keyStr += 'S'; break; }
      else if (key == 'D' && !keyStr.isEmpty()) { keyStr += 'W'; break; }
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

  return keyStr;
}

String getDirection(JsonObject routeObj, const char* stopTag) {
  bool isNorth = false;
  bool isEast = false;
  bool isSouth = false;
  bool isWest = false;

  for (JsonObject dir: routeObj["direction"].as<JsonArray>()) {
    const char* dirName = dir["name"];

    for (JsonObject stopRef: dir["stop"].as<JsonArray>()) {
      const char* tag = stopRef["tag"];
      if (strcmp(tag, stopTag) == 0) {
        if (strcmp(dirName, "North") == 0) isNorth = true;
        else if (strcmp(dirName, "East") == 0) isEast = true;
        else if (strcmp(dirName, "South") == 0) isSouth = true;
        else if (strcmp(dirName, "West") == 0) isWest = true; 
      }
    }
  }
  if (isNorth) return "N";
  else if (isEast) return "E";
  else if (isSouth) return "S";
  else if (isWest) return "W";
  return "None";
}

float getDistance(float lat1, float lng1, float lat2, float lng2) {
  const float R = 6371.0;
  float dLat = (lat2 - lat1) * M_PI / 180.0;
  float dLng = (lng2 - lng1) * M_PI / 180.0;
  float lat1_rad = lat1 * M_PI / 180.0;
  float lat2_rad = lat2 * M_PI / 180.0;

  float a = pow(sin(dLat / 2), 2) + cos(lat1_rad) * cos(lat2_rad) * pow(sin(dLng / 2), 2);
  float c = 2 * atan2(sqrt(a), sqrt(1 - a));
  float d = R * c;
  return d;
}

void getBusRoute(String route) {
  char busDir = route.charAt(route.length() - 1);
  String copyRoute = route;
  copyRoute.remove(copyRoute.length() - 1);
  String busNum = copyRoute;
  Serial.println("Fetching the bus stops for the route");
  if (WiFi.status() == WL_CONNECTED) {
    String url = "https://webservices.umoiq.com/service/publicJSONFeed?command=routeConfig&a=ttc&r=" + busNum;

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;

    Serial.println("Using the bus route url");
    // Serial.println(url);
    if (http.begin(client, url)) {
      int httpCode = http.GET();
      if (httpCode > 0) {
        String payload = http.getString();
        Serial.println("Response: ");
        Serial.println(payload);

        JsonDocument doc ;
        DeserializationError error = deserializeJson(doc, payload);
        if (error) {
          Serial.print(F("deserializeJson() failed"));
          Serial.print(error.c_str());
          return;
        }

        JsonObject routeObj = doc["route"];
        if (!routeObj) {
          Serial.println("No route found");
          LCD.clear();
          LCD.setCursor(0, 1);
          LCD.print("No route!");
          return;
        }

        float lat2, lng2;
        getLocation(&lat2, &lng2);
        String closestStopId = "";
        float closestDist = 999999.0;
        float closestLat = 0.0;
        float closestLng = 0.0;

        for (JsonObject stop : routeObj["stop"].as<JsonArray>()) {
          const char* stopTag = stop["tag"];
          const char* lat = stop["lat"];
          const char* lng = stop["lon"];
          String dir = getDirection(routeObj, stopTag);
          if (dir == String(busDir)) {
            Serial.println("Tag: " + String(stopTag) + " | Latitude: " + String(lat) + " | Longitude: " + String(lng) + " | Direction: " + dir);
            float lat1 = atof(lat);
            float lng1 = atof(lng);
            float dist = getDistance(lat1, lng1, lat2, lng2);
            if (dist < closestDist) {
              closestDist = dist;
              closestLat = lat1;
              closestLng = lng1;
              closestStopId = stop["stopId"].as<String>();
            }
          }
        }
        
        if (closestStopId != "") {
          Serial.println("StopId: " + closestStopId + " | Distance: " + String(closestDist) + "km | Latitude: " + String(closestLat) + " | Longitude: " + String(closestLng));
          getBus(closestStopId);
        } else {
          Serial.println("No stops found for " + String(busDir));
          LCD.clear();
          LCD.print("No stop!");
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
  LCD.print("Online");
  LCD.setCursor(0, 1);
  String busNum = getBusNum();
  getBusRoute(busNum);
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

  // if (millis() - lastUpdated > busInterval) {
  //   getBus();
  //   lastUpdated = millis();
  // }
}
