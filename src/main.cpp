
#include <Arduino.h>
#include <Preferences.h>

// ==================== PIN CONFIGURATION ====================

constexpr uint8_t CROWN_BUTTON_PIN = 18;
constexpr uint8_t FRONT_TOUCH_PIN = 4;

constexpr uint8_t GREEN_LED_PIN = 25;
constexpr uint8_t RED_LED_PIN = 26;
constexpr uint8_t BLUE_LED_PIN = 27;

// ==================== TASK CONFIGURATION ====================

constexpr uint16_t TOTAL_SAMPLES = 2000;
constexpr uint8_t WINDOW_SIZE = 16;
constexpr uint8_t FLOOR_COUNT = 8;
constexpr uint8_t VALID_TAG = 0xA5;

constexpr uint32_t DEBOUNCE_MS = 30;
constexpr uint32_t LONG_PRESS_MS = 2000;
constexpr uint32_t DOUBLE_TAP_MIN_MS = 200;
constexpr uint32_t DOUBLE_TAP_MAX_MS = 500;

// ==================== ENGINE STATE ====================

struct EngineState {
    uint16_t rawBuffer[WINDOW_SIZE] = {};
    uint8_t writeIndex = 0;
    uint8_t sampleCount = 0;
    uint16_t rollingPeak = 0;
    uint16_t rollingFloor = 0;
};

EngineState engine;

static_assert(
    sizeof(((EngineState*)nullptr)->rawBuffer) < 64,
    "Raw buffer must be less than 64 bytes"
);


void printMemoryReport() {
    Serial.println();
    Serial.println("==============================================");
    Serial.println("       ENGINE STATE MEMORY REPORT");
    Serial.println("==============================================");

    Serial.printf("%-18s %-12s %10s\n",
                  "Variable", "Data Type", "Size (B)");

    Serial.println("----------------------------------------------");

    Serial.printf("%-18s %-12s %10u\n",
                  "rawBuffer[16]", "uint16_t[16]",
                  (unsigned)sizeof(engine.rawBuffer));

    Serial.printf("%-18s %-12s %10u\n",
                  "writeIndex", "uint8_t",
                  (unsigned)sizeof(engine.writeIndex));

    Serial.printf("%-18s %-12s %10u\n",
                  "sampleCount", "uint8_t",
                  (unsigned)sizeof(engine.sampleCount));

    Serial.printf("%-18s %-12s %10u\n",
                  "rollingPeak", "uint16_t",
                  (unsigned)sizeof(engine.rollingPeak));

    Serial.printf("%-18s %-12s %10u\n",
                  "rollingFloor", "uint16_t",
                  (unsigned)sizeof(engine.rollingFloor));

    Serial.println("----------------------------------------------");

    size_t rawBufferSize = sizeof(engine.rawBuffer);
    size_t totalSize = sizeof(EngineState);

    Serial.printf("%-18s %-12s %10u\n",
                  "EngineState TOTAL", "-", (unsigned)totalSize);

    Serial.println();
    Serial.println("========== FINAL MEMORY RESULTS ==============");

    Serial.printf("EngineState size : %u bytes\n",
                  (unsigned)totalSize);

    Serial.printf("EngineState limit: 512 bytes\n");

    Serial.printf("Raw buffer size  : %u bytes\n",
                  (unsigned)rawBufferSize);

    Serial.printf("Raw buffer limit : less than 64 bytes\n");

    Serial.printf("EngineState      : %s\n",
                  totalSize <= 512 ? "PASS" : "FAIL");

    Serial.printf("Raw pulse buffer : %s\n",
                  rawBufferSize < 64 ? "PASS" : "FAIL");

    Serial.println("==============================================");
}


// The checkpoint stores the sample index, rolling average and tag.
struct Checkpoint {
    uint16_t sampleIndex;
    uint16_t rollingAverage;
    uint8_t validationTag;
};


Preferences preferences;

uint16_t nextSampleIndex = 0;
bool halted = false;
bool greenState = false;
bool redState = false;

enum WatchMode {
    ACTIVE_MODE,
    STEALTH_MODE
};

WatchMode watchMode = ACTIVE_MODE;

// ==================== BUTTON STATE ====================

bool lastCrownReading = HIGH;
bool stableCrownState = HIGH;
uint32_t crownChangeTime = 0;

bool lastTouchReading = HIGH;
bool stableTouchState = HIGH;
uint32_t touchChangeTime = 0;

uint32_t firstTapTime = 0;
bool waitingForSecondTap = false;

uint32_t touchPressStart = 0;
bool longPressHandled = false;

// ==================== SIMULATED PPG DATA ====================

// Deterministic simulated 16-bit samples.
// Replace this function with sensor input when hardware is available.
uint16_t getSample(uint16_t index) {
    uint16_t phase = index % 40;

    return 2000 + ((phase < 20) ? phase : 40 - phase);
}

// ==================== PULSE PROCESSING ====================

void processSample(EngineState &state, uint16_t sample) {
    // Add the incoming sample to the circular buffer.
    state.rawBuffer[state.writeIndex] = sample;
    state.writeIndex = (state.writeIndex + 1) % WINDOW_SIZE;

    if (state.sampleCount < WINDOW_SIZE) {
        state.sampleCount++;
    }

    // Calculate peak in the current window.
    uint16_t peak = 0;

    for (uint8_t i = 0; i < state.sampleCount; i++) {
        if (state.rawBuffer[i] > peak) {
            peak = state.rawBuffer[i];
        }
    }

    state.rollingPeak = peak;

    // Sort current samples to calculate the average of the
    // lowest 8 values (or all available values if fewer than 8).
    uint16_t sorted[WINDOW_SIZE];

    for (uint8_t i = 0; i < state.sampleCount; i++) {
        sorted[i] = state.rawBuffer[i];
    }

    for (uint8_t i = 1; i < state.sampleCount; i++) {
        uint16_t key = sorted[i];
        int16_t j = (int16_t)i - 1;

        while (j >= 0 && sorted[j] > key) {
            sorted[j + 1] = sorted[j];
            j--;
        }

        sorted[j + 1] = key;
    }

    uint8_t count = state.sampleCount;

    if (count > FLOOR_COUNT) {
        count = FLOOR_COUNT;
    }

    uint32_t sum = 0;

    for (uint8_t i = 0; i < count; i++) {
        sum += sorted[i];
    }

    state.rollingFloor = (uint16_t)(sum / count);
}

// ==================== LED INDICATORS ====================

void updateLEDs() {
    if (watchMode == STEALTH_MODE) {
        digitalWrite(GREEN_LED_PIN, LOW);
        digitalWrite(RED_LED_PIN, LOW);
        digitalWrite(BLUE_LED_PIN, LOW);
        return;
    }

    digitalWrite(GREEN_LED_PIN, greenState ? HIGH : LOW);
    digitalWrite(RED_LED_PIN, redState ? HIGH : LOW);
    digitalWrite(BLUE_LED_PIN, HIGH);
}

void printWatchMode() {
    Serial.print("CURRENT MODE: ");

    if (watchMode == ACTIVE_MODE) {
        Serial.println("ACTIVE_MODE");
    } else {
        Serial.println("STEALTH_MODE");
    }
}

// ==================== CHECKPOINT STORAGE ====================

void saveCheckpoint() {
    Checkpoint cp;

    cp.sampleIndex = nextSampleIndex;
    cp.rollingAverage = engine.rollingFloor;
    cp.validationTag = VALID_TAG;

    size_t written = preferences.putBytes(
        "checkpoint",
        &cp,
        sizeof(cp)
    );

    Serial.println();
    Serial.println("=== BROWNOUT SIMULATED ===");

    if (written == sizeof(cp)) {
        Serial.printf(
            "Saved sample index: %u\n",
            cp.sampleIndex
        );

        Serial.printf(
            "Saved rolling average: %u\n",
            cp.rollingAverage
        );

        Serial.println("Validation tag: 0xA5");
        Serial.println("Checkpoint saved successfully.");
    } else {
        Serial.println("ERROR: Checkpoint save failed.");
    }

    halted = true;
    updateLEDs();

    Serial.println("Watch halted. Reset to recover.");
}

bool loadCheckpoint() {
    if (!preferences.isKey("checkpoint")) {
        Serial.println("No checkpoint found.");
        return false;
    }

    if (preferences.getBytesLength("checkpoint") != sizeof(Checkpoint)) {
        Serial.println("Invalid checkpoint size.");
        return false;
    }

    Checkpoint cp;

    size_t bytesRead = preferences.getBytes(
        "checkpoint",
        &cp,
        sizeof(cp)
    );

    if (bytesRead != sizeof(cp)) {
        Serial.println("Checkpoint read failed.");
        return false;
    }

    if (cp.validationTag != VALID_TAG) {
        Serial.println("Invalid checkpoint tag.");
        return false;
    }

    if (cp.sampleIndex > TOTAL_SAMPLES) {
        Serial.println("Invalid checkpoint sample index.");
        return false;
    }

    // Rebuild the rolling window deterministically.
    engine = EngineState();

    for (uint16_t i = 0; i < cp.sampleIndex; i++) {
        processSample(engine, getSample(i));
    }

    nextSampleIndex = cp.sampleIndex;

    Serial.println("Valid checkpoint found.");

    Serial.printf(
        "Resuming from sample index: %u\n",
        nextSampleIndex
    );

    Serial.printf(
        "Saved rolling average: %u\n",
        cp.rollingAverage
    );

    Serial.printf(
        "Rebuilt rolling average: %u\n",
        engine.rollingFloor
    );

    if (cp.rollingAverage != engine.rollingFloor) {
        Serial.println("WARNING: Checkpoint average mismatch.");
    }

    return true;
}

// ==================== TOUCH AND CROWN GESTURES ====================

void handleGestures() {
    uint32_t now = millis();

    // ----- Side crown: debounced press simulates brownout -----

    bool crownReading = digitalRead(CROWN_BUTTON_PIN);

    if (crownReading != lastCrownReading) {
        lastCrownReading = crownReading;
        crownChangeTime = now;
    }

    if ((now - crownChangeTime >= DEBOUNCE_MS) &&
        (crownReading != stableCrownState)) {

        stableCrownState = crownReading;

        // INPUT_PULLUP: LOW means pressed.
        if (stableCrownState == LOW && !halted) {
            saveCheckpoint();
            return;
        }
    }

    // ----- Front touch: debounced tap detection -----

    bool touchReading = digitalRead(FRONT_TOUCH_PIN);

    if (touchReading != lastTouchReading) {
        lastTouchReading = touchReading;
        touchChangeTime = now;
    }

    if ((now - touchChangeTime >= DEBOUNCE_MS) &&
        (touchReading != stableTouchState)) {

        stableTouchState = touchReading;

        if (stableTouchState == LOW) {
            // Button pressed.
            touchPressStart = now;
            longPressHandled = false;
        } else {
            // Button released.
            uint32_t heldTime = now - touchPressStart;

            if (longPressHandled || heldTime >= LONG_PRESS_MS) {
                waitingForSecondTap = false;
            } else {
                if (waitingForSecondTap) {
                    uint32_t gap = now - firstTapTime;

                    if (gap >= DOUBLE_TAP_MIN_MS &&
                        gap <= DOUBLE_TAP_MAX_MS) {

                        watchMode =
                            (watchMode == ACTIVE_MODE)
                            ? STEALTH_MODE
                            : ACTIVE_MODE;

                        waitingForSecondTap = false;

                        printWatchMode();
                        updateLEDs();
                    } else {
                        // This tap becomes the first tap
                        // of a new double-tap sequence.
                        firstTapTime = now;
                        waitingForSecondTap = true;
                    }
                } else {
                    firstTapTime = now;
                    waitingForSecondTap = true;
                }
            }
        }
    }

    // ----- Long press forces ACTIVE_MODE -----

    if (stableTouchState == LOW &&
        !longPressHandled &&
        (now - touchPressStart >= LONG_PRESS_MS)) {

        longPressHandled = true;
        waitingForSecondTap = false;

        watchMode = ACTIVE_MODE;
        updateLEDs();

        Serial.println("LONG PRESS DETECTED");
        printWatchMode();
    }

    // Expire an incomplete double-tap sequence.
    if (waitingForSecondTap &&
        (now - firstTapTime > DOUBLE_TAP_MAX_MS)) {

        waitingForSecondTap = false;
    }
}

// ==================== SETUP ====================

void setup() {
    Serial.begin(115200);
    delay(300);
    printMemoryReport();

    pinMode(CROWN_BUTTON_PIN, INPUT_PULLUP);
    pinMode(FRONT_TOUCH_PIN, INPUT_PULLUP);

    pinMode(GREEN_LED_PIN, OUTPUT);
    pinMode(RED_LED_PIN, OUTPUT);
    pinMode(BLUE_LED_PIN, OUTPUT);

    digitalWrite(GREEN_LED_PIN, LOW);
    digitalWrite(RED_LED_PIN, LOW);
    digitalWrite(BLUE_LED_PIN, LOW);

    preferences.begin("ppg-watch", false);

    Serial.println();
    Serial.println("=== SMARTWATCH STARTING ===");

    if (!loadCheckpoint()) {
        nextSampleIndex = 0;
        engine = EngineState();

        Serial.println("Starting from sample 0.");
    }

    watchMode = ACTIVE_MODE;
    halted = false;

    Serial.printf(
        "Raw buffer size: %u bytes\n",
        (unsigned)sizeof(engine.rawBuffer)
    );

    printWatchMode();
    updateLEDs();
}

// ==================== MAIN LOOP ====================

void loop() {
    handleGestures();

    // After a crown press, stop processing until reset.
    if (halted) {
        delay(10);
        return;
    }

    if (nextSampleIndex < TOTAL_SAMPLES) {
        // Process exactly one sample per iteration.
        uint16_t sample = getSample(nextSampleIndex);

        processSample(engine, sample);

        // Green LED toggles every 50 processed samples.
        if (nextSampleIndex % 50 == 49) {
            greenState = !greenState;
        }

        // Red LED ON at 150% or more of the rolling average.
        uint32_t threshold =
            ((uint32_t)engine.rollingFloor * 3) / 2;

        redState = sample >= threshold;

        nextSampleIndex++;

        updateLEDs();

        if (nextSampleIndex % 50 == 0) {
            Serial.printf(
                "Samples: %u/%u | Latest: %u | "
                "Rolling Peak: %u | Rolling Average: %u\n",
                nextSampleIndex,
                TOTAL_SAMPLES,
                sample,
                engine.rollingPeak,
                engine.rollingFloor
            );
        }
    } else {
        Serial.println();
        Serial.println("=== ALL 2000 SAMPLES PROCESSED ===");

        Serial.printf(
            "Final rolling peak: %u\n",
            engine.rollingPeak
        );

        Serial.printf(
            "Final rolling average: %u\n",
            engine.rollingFloor
        );

        halted = true;
    }

    delay(5);
}
