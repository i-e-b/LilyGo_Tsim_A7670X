#include "Arduino.h"

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("Hello, world!");
}

int i = 0;
void loop() {
  delay(1000);
  Serial.printf("Device is active (%d) \r\n", i++);
}
