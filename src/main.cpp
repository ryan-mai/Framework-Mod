#include <Arduino.h>

#include <math.h>
#include <time.h>

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>


#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#include <Keypad.h>

#include "secrets.h"

const char* city = "Toronto";
const char* timezone = "EST5EDT,M3.2.0/2,M11.1.0";

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
const long busInterval = 30067; // 30.67 secs;
const long connectionDelay = 500;

bool isBus = false;
bool cancelBus = false;
String busId = "";
bool enteringBus = false;

int lastWeatherUpdated = 0;
int weatherInterval = 300000;
int tempLen = 3;

float user_lat, user_lng;

int yesterday = -1;
int lastTimeUpdated = 0;
const unsigned long timeInterval = 60000;

void getTime() {
  if (WiFi.status() == WL_CONNECTED) {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
      LCD.setCursor(7,0);
      LCD.print("Time Fail ");
      Serial.println("Time failed");
      delay(1000);
      return;
    }
    
    char dd[10];
    char clock[12];
    strftime(dd, sizeof(dd), "%a %m-%d", &timeinfo);
    strftime(clock, sizeof(clock), "%I:%M", &timeinfo);
    Serial.println("Day: " + String(dd) + " | Time: " + String(clock));

    if (timeinfo.tm_mday != yesterday) {
      LCD.setCursor(7, 1);
      LCD.print("         ");
      LCD.setCursor(7, 1);
      LCD.print(dd);
      yesterday = timeinfo.tm_mday;
    }

    int timeCursor = 16 - tempLen - 6;
    LCD.setCursor(timeCursor, 0);
    LCD.print("     ");
    LCD.setCursor(timeCursor, 0);
    LCD.print(clock);
  
  } else {
    LCD.setCursor(7, 0);
    LCD.print("WiFi Fail ");
    Serial.println("Wifi not connected for the time");
  }
}

bool getLocation(float* latOut, float* lngOut) {
  *latOut = 0.0f;
  *lngOut = 0.0f;
  
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClientSecure client;
    client.setInsecure();
    int networks = WiFi.scanNetworks();
    JsonDocument doc;
    doc.clear();
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


void getWeather() {
  Serial.println("Accessing the weather api");
  if (WiFi.status() == WL_CONNECTED) {
    
    if (!getLocation(&user_lat, &user_lng)) {
      LCD.setCursor(13, 0);
      LCD.print("Loc Fail");
      Serial.println("Location could not be found");
      return;
    }

    String loc = String(user_lat) + "," + String(user_lng);

    String url = "https://api.weatherapi.com/v1/current.json?key=" + String(weatherApiKey) + "&q=" + loc + "&aqi=no";;

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

        char tempStr[10];
        sprintf(tempStr, "%.0fC", temp_c);
        int len = strlen(tempStr);
        tempLen = len;
        int cursorPos = 16 - len;
        LCD.setCursor(cursorPos, 0);
        LCD.print(tempStr);

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

void clearRow(uint8_t row) {
  LCD.setCursor(0, row);
  LCD.print("                ");
}

void showStatus(const char* msg) {
  clearRow(1);
  LCD.setCursor(0, 1);
  LCD.print(msg);
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
          showStatus("BUS JSON ERR");
          return;
        }

        JsonObject predictionsObj = doc["predictions"].as<JsonObject>();
        if (predictionsObj.isNull() || predictionsObj.size() == 0) {
          Serial.println("No prediction data");
          showStatus("No Bus Data!");
          return;
        }

        JsonObject routeObj = predictionsObj["direction"].as<JsonObject>();
        if (routeObj.isNull()) {
          Serial.println("No directions found");
          showStatus("No Directions!");
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
          showStatus("N/A");
          return;
        }
        if (predictions.size() == 0) {
          showStatus("No upcoming!");
          Serial.println("No upcoming!");
          return;
        }
        isBus = true;
        busId = stopId;

        float time0 = predictions[0]["seconds"].as<float>();
        int total_seconds = static_cast<int>(std::round(time0));
        int seconds = total_seconds % 60;
        int minutes = total_seconds / 60;

        char buf[17];
        snprintf(buf, sizeof(buf), "%d:%02d", minutes, seconds);
        clearRow(1);
        LCD.setCursor(0, 1);
        LCD.print(buf);

        if (predictions.size() > 1) {
          float time1 = predictions[1]["seconds"].as<float>();
          int total_seconds_next = static_cast<int>(std::round(time1));
          int seconds_next = total_seconds_next % 60;
          int minutes_next = total_seconds_next / 60;

          // Append "  next m:ss"
          char buf2[17];
          snprintf(buf2, sizeof(buf2), " %d:%02d", minutes_next, seconds_next);
          LCD.print(buf2);
        }
      } else {
        Serial.printf("http.GET() failed, error %d\n", httpCode);
        showStatus("BUS ERR");
      }
      http.end();
    } else {
      Serial.println("Could not make HTTP request");
      showStatus("REQ FAIL");
    }
  } else {
    Serial.println("WiFi not connected");
    showStatus("WIFI!");
  }
}
String getBusNum() {
  clearRow(1);
  LCD.setCursor(0, 1);
  LCD.print("Bus: ");
  
  String keyStr = "";
  
  while (true) {
    char key = keypad.getKey();
    if (key != NO_KEY) {
      if (key >= '0' && key <= '9') {
        keyStr += key;
      } 
      else if (key == '*') {
        if (!keyStr.isEmpty()) keyStr.remove(keyStr.length() - 1);
      } 
      else if (key == '#') {
        clearRow(1);
        return "CANCEL";
      }
      else if (key == 'A' || key == 'B' || key == 'C' || key == 'D') {
        if (keyStr.isEmpty()) continue;
        if (key == 'A') keyStr += 'N';
        if (key == 'B') keyStr += 'E';
        if (key == 'C') keyStr += 'S';
        if (key == 'D') keyStr += 'W';
        clearRow(1);
        LCD.setCursor(0, 0);
        LCD.print("GET ");
        LCD.print(keyStr);
        delay(800);
        clearRow(1);
        return keyStr;
      }

      LCD.setCursor(5, 0);
      LCD.print("            ");
      LCD.setCursor(5, 0);
      LCD.print(keyStr);
    }
    delay(50);
  }
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
          showStatus("JSON ERR");
          return;
        }

        JsonObject routeObj = doc["route"];
        if (!routeObj) {
          Serial.println("No route found");
          showStatus("No Route");
          return;
        }

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
            float dist = getDistance(user_lat, user_lng, lat1, lng1);
            if (dist < closestDist) {
              closestDist = dist;
              closestLat = lat1;
              closestLng = lng1;
              String stopIdChar = stop["stopId"].as<String>();
              if (stopIdChar) {
                closestStopId = stopIdChar;
              }
            }
          }
        }
        
        if (closestStopId != "") {
          Serial.println("StopId: " + closestStopId + " | Distance: " + String(closestDist) + "km | Latitude: " + String(closestLat) + " | Longitude: " + String(closestLng));
          getBus(closestStopId);
        } else {
          Serial.println("No stops found for " + String(busDir));
          showStatus("No Stop");
        }
      } else {
        Serial.printf("http.GET() failed, error %d\n", httpCode);
        showStatus("HTTP ERR");
      }
      http.end();
    } else {
      Serial.println("Could not make HTTP request");
      showStatus("Req Fail");
    }
  } else {
    Serial.println("WiFi not connected");
    showStatus("WiFi ERR");
  }
}

String nf(int num) {
  if (num < 10) return "0" + String(num);
  return String(num);
}

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
  LCD.print("ON");

  configTzTime(timezone, "pool.ntp.org", "time.nist.gov", "time.google.com");
  Serial.println("Loading time");
  LCD.setCursor(7, 1);
  LCD.print("--- -----");
  LCD.setCursor(7, 0);
  LCD.print("--:--    ");

    while (time(nullptr) < 1000000000L) {
      delay(200);
      Serial.print("Waiting to load time...");
    }
    Serial.println("Time loaded!");

  getTime();
  getWeather();
  lastUpdated = millis();
  lastWeatherUpdated = millis();
  lastTimeUpdated = millis();
  clearRow(1);
}

void loop() {
  char key = keypad.getKey();
  if (key == '#') {
      cancelBus = true;
  }

  if (!isBus && !enteringBus && key >= '0' && key <= '9') {
    enteringBus = true;
  }

  if (enteringBus) {
    String busNum = getBusNum();
    if (busNum != "CANCEL" && !busNum.isEmpty()) {
      getBusRoute(busNum);
    }
    enteringBus = false;
  }

  if (millis() - lastWeatherUpdated > weatherInterval) {
    getWeather();
    lastWeatherUpdated = millis();
  }

  if (millis() - lastTimeUpdated > timeInterval) {
    getTime();
    lastTimeUpdated = millis();
  }

  if (isBus) {
    if (millis() - lastUpdated > busInterval) {
      getBus(busId);
      lastUpdated = millis();
    }
  }
}