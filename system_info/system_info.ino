#include <TimeLib.h>

#include <Wire.h>
#include <LiquidCrystal.h>

// Setup
// LiquidCrystal_I2C lcd(0x3F, 16, 2);


void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  while (!Serial);

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

  static unsigned long lastShown = 0;
  if (millis() - lastShown > 1000) {
    lastPrint = millis();
    Serial.print("-----")
    Serial.println(formatDate());
  }

}

String formateDate() {
  return String(year()) + "-" + nf(month(), 2) + "-" + nf(day(), 2) + " " + nf(hour(), 2) + ":" + (minute(), 2) + ":" + (second(), 2); 
}
