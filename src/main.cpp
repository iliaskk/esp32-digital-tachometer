#include <Arduino.h>
#include <TFT_eSPI.h>
#include <cinttypes>

#include <atomic>  //++
#include <cstdint> //++

TFT_eSPI tft = TFT_eSPI();

const uint8_t sparkPin = 25;                 // Pin connected to the spark signal
const uint32_t microsPerMinute = 60'000'000; //'; magic number avoiding
const uint32_t engineTimeoutLimit = 1'000'000;
const uint32_t impossibleInitialRpm = 9999;
const uint32_t displayRefreshRate = 100;
const uint32_t maxEngineRpm = 9500;
const uint32_t debounceDelay = microsPerMinute / maxEngineRpm;

// Data Encapsulation (Το "κουτί" με τις μεταβλητές που αλλάζουν)
struct EngineData
{
    volatile uint32_t lastSparkTime;
    volatile uint32_t timeBetweenSparks;
    uint32_t currentRpm;
    uint32_t lastRpmState;
    uint32_t lastDisplayTime;
};

// Αρχικοποίηση του struct με τις αρχικές τιμές
EngineData engine = {0, 0, 0, impossibleInitialRpm, 0};

// ISR Background
void IRAM_ATTR recordPulse()
{
    uint32_t localIsrTime = micros();
    uint32_t localSparkNew = localIsrTime - engine.lastSparkTime;

    if (localSparkNew > debounceDelay)
    {
        engine.timeBetweenSparks = localSparkNew;
        engine.lastSparkTime = localIsrTime;
    }
}

void calculateRpm()
{
    // we use local variables so we can run calculateRpm() and then pass the data to the engine struct to achieve State Management
    uint32_t localLastSparkTime = 0;
    uint32_t localTimeBetweenSparks = 0;
    uint32_t localCurrentRpm = 0;
    uint32_t localCurrentTime = micros();

    noInterrupts();
    localLastSparkTime = engine.lastSparkTime;
    localTimeBetweenSparks = engine.timeBetweenSparks;
    interrupts();

    if ((localCurrentTime - localLastSparkTime > engineTimeoutLimit) || (localTimeBetweenSparks == 0))
    {

        localCurrentRpm = 0; // engine stopped

        noInterrupts();
        engine.lastSparkTime = localCurrentTime; // Prevents micros() 71-min rollover bug
        interrupts();
    }
    else
    {
        localCurrentRpm = microsPerMinute / localTimeBetweenSparks;
    }

    engine.currentRpm = localCurrentRpm;
}

// Display UI; Change-Detection State for Anti-flickering
void updateDisplay()
{
    if (engine.currentRpm != engine.lastRpmState) // if rpm has changed, update the display
    {
        tft.setCursor(10, 40);
        tft.print("RPM: ");
        tft.print(engine.currentRpm);
        tft.print("   "); // works like a "smart eraser"
        Serial.printf("RPM: %" PRIu32 "\n", engine.currentRpm);
        engine.lastRpmState = engine.currentRpm;
    }
}

// Initial Setup
void setup()
{
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextSize(2);
    Serial.begin(115200);

    tft.setCursor(10, 10);
    tft.println("System Ready!");

    pinMode(sparkPin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(sparkPin), recordPulse, RISING); // Σύνδεσε έναν συναγερμό στο Pin 25. Μόλις δεις την τάση να ανεβαίνει (σπινθήρας), τρέξε τη συνάρτηση recordPulse για να αυξήσεις τον μετρητή
} // attachInterrupt; an alarm

void loop()
{
    uint32_t localCurrentLoopTime = millis(); // Δεν βάζουμε ΠΟΤΕ την ίδια μεταβλητή να διαβάζει micros() (εκατομμυριοστά) και μετά από λίγο millis()

    if (localCurrentLoopTime - engine.lastDisplayTime >= displayRefreshRate)
    {
        calculateRpm();
        updateDisplay();
        engine.lastDisplayTime = localCurrentLoopTime; // Αποθήκευση χρόνου τελευταίας ενημέρωσης
    }
}