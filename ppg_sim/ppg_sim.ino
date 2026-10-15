#include <Arduino.h>

const int PPG_PIN = 36;      // GPIO36 / ADC1_CH0
int threshold = 2200;       // Tweak based on your virtual baseline signal
unsigned long lastBeat = 0;
bool signalAboveThreshold = false;

void setup() {
    Serial.begin(115200);
    pinMode(PPG_PIN, INPUT);
}

void loop() {
    int ppgValue = analogRead(PPG_PIN); // Reads raw 12-bit ADC data (0-4095)
    
    // Print to Serial Plotter to visualize simulated arterial pulse waves
    Serial.print("PPG_Signal:");
    Serial.println(ppgValue);

    // Basic Peak Detection Logic
    if (ppgValue > threshold && !signalAboveThreshold) {
        signalAboveThreshold = true;
        unsigned long currentMillis = millis();
        unsigned long duration = currentMillis - lastBeat;
        
        if (duration > 400 && duration < 2000) { // Keep bounds between 30 and 150 BPM
            float bpm = 60000.0 / duration;
            Serial.print("---> Beat Detected! Estimated BPM: ");
            Serial.println(bpm);
        }
        lastBeat = currentMillis;
    } 
    else if (ppgValue < (threshold - 100)) { // Add hysteresis to avoid noise bouncing
        signalAboveThreshold = false;
    }
    
    delay(20); // 50Hz sample sampling rate
}
