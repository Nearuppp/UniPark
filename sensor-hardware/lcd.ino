#include <Wire.h>
#include "rgb_lcd.h"

rgb_lcd lcd;

void setup() {
  lcd.begin(16, 2);                // Initialize LCD as 16x2
  lcd.setRGB(0, 128, 255);         // Set backlight color (blueish)
  lcd.print("Hello Matthieu!");   // Display message
  delay(2000);
  lcd.setCursor(0, 1);             // Move to second line
  lcd.print("Keyestudio Mega");
}

void loop() {
  // Nothing in loop
}
