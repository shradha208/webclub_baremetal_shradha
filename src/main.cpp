
#include <Arduino.h>
#include <stdint.h>

constexpr uint8_t WINDOW_SIZE = 16;
constexpr uint8_t FLOOR_COUNT = 8;

struct EngineState {
    // 16 samples x 2 bytes = 32 bytes
    uint16_t rawBuffer[WINDOW_SIZE] = {};
    uint8_t writeIndex = 0;
    uint8_t sampleCount = 0;
    uint16_t rollingPeak = 0;
    uint16_t rollingFloor = 0;
};

static_assert(
    sizeof(((EngineState*)nullptr)->rawBuffer) < 64,
    "Raw buffer must be less than 64 bytes"
);

EngineState engine;

void processSample(EngineState &s, uint16_t sample) {
    // Store the incoming sample in the rolling buffer
    s.rawBuffer[s.writeIndex] = sample;
    s.writeIndex = (s.writeIndex + 1) % WINDOW_SIZE;

    if (s.sampleCount < WINDOW_SIZE) {
        ++s.sampleCount;
    }

    // Calculate peak and sort current samples
    uint16_t sorted[WINDOW_SIZE] = {};
    s.rollingPeak = 0;

    for (uint8_t i = 0; i < s.sampleCount; ++i) {
        uint16_t value = s.rawBuffer[i];

        if (value > s.rollingPeak) {
            s.rollingPeak = value;
        }

        // Insertion sort
        uint8_t j = i;
        while (j > 0 && sorted[j - 1] > value) {
            sorted[j] = sorted[j - 1];
            --j;
        }
        sorted[j] = value;
    }

    // Average the 8 lowest values to estimate the pulse floor
    uint8_t count = (s.sampleCount < FLOOR_COUNT)
                        ? s.sampleCount
                        : FLOOR_COUNT;

    uint32_t sum = 0;
    for (uint8_t i = 0; i < count; ++i) {
        sum += sorted[i];
    }

    s.rollingFloor = (count > 0) ? sum / count : 0;
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("Starting PPG pulse monitor...");

    // Generate and process 2000 simulated 16-bit samples
    for (uint16_t i = 0; i < 2000; ++i) {
        uint16_t phase = i % 40;

        uint16_t sample = 2000 +
            ((phase < 20) ? phase : 40 - phase);

        processSample(engine, sample);
    }

    Serial.println("Processed 2000 samples.");

    Serial.printf(
        "Raw buffer size: %u bytes\n",
        (unsigned)sizeof(engine.rawBuffer)
    );

    Serial.printf("Rolling peak: %u\n", engine.rollingPeak);
    Serial.printf("Rolling floor: %u\n", engine.rollingFloor);
    Serial.println("PPG processing complete.");
}


void loop() {
    Serial.println("PPG monitor is running");
    Serial.printf("Raw buffer size: %u bytes\n",
                  (unsigned)sizeof(engine.rawBuffer));
    Serial.printf("Rolling peak: %u\n", engine.rollingPeak);
    Serial.printf("Rolling floor: %u\n", engine.rollingFloor);
    Serial.println("--------------------");

    delay(2000);
}


