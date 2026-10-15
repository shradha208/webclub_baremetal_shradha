#include <Wire.h>
#include "MAX30105.h" // Standard library for MAX30102/5

MAX30105 particleSensor;

void setup() {
  Serial.begin(115200);
  
  // Initialize I2C communication
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("PPG Sensor not found. Check wiring!");
    while (1);
  }
  
  particleSensor.setup(); // Configure sensor with default settings
}

void loop() {
  // Read the Red or Infrared led value (PPG data)
  long irValue = particleSensor.getIR(); 
  
  if (irValue > 50000) {
    Serial.print("PPG IR Signal: ");
    Serial.println(irValue);
  } else {
    Serial.println("No finger detected");
  }
  delay(20);
}
