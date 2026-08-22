#include <Arduino.h>
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

const uint8_t sparkPin = 25;                      // Pin connected to the spark signal
const unsigned long debounceDelay = 3000;         // 3000 microseconds = 3 milliseconds
const unsigned long microsPerMinute = 60'000'000; //'; magic number avoiding

// Data Encapsulation (Το "κουτί" με τις μεταβλητές που αλλάζουν)
struct EngineData
{
    volatile unsigned long lastSparkTime;
    volatile unsigned long timeBetweenSparks;
    unsigned long currentRpm;
    unsigned long lastRpmState;
    unsigned long lastDisplayTime;
};

// Αρχικοποίηση του struct με τις αρχικές τιμές
EngineData engine = {0, 0, 0, 9999, 0}; // 9999 is an Impossible Initial State

// ISR Background
void IRAM_ATTR recordPulse()
{
    unsigned long currentIsrTime = micros();
    unsigned long sparkNew = currentIsrTime - engine.lastSparkTime;

    if (sparkNew > debounceDelay)
    {
        engine.timeBetweenSparks = sparkNew;
        engine.lastSparkTime = currentIsrTime;
    }
}

void calculateRPM()
{
    noInterrupts();
    if (micros() - engine.lastSparkTime > 1000000) // 1 second
    {

        engine.currentRpm = 0; // engine stopped
    }
    else if (engine.timeBetweenSparks > 0)
    {
        engine.currentRpm = microsPerMinute / engine.timeBetweenSparks;
    }
    else
    {
        engine.currentRpm = 0;
    }
    interrupts();
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
        Serial.printf("RPM: %lu\n", engine.currentRpm);
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
    unsigned long currentLoopTime = millis(); // Δεν βάζουμε ΠΟΤΕ την ίδια μεταβλητή να διαβάζει micros() (εκατομμυριοστά) και μετά από λίγο millis()

    if (currentLoopTime - engine.lastDisplayTime >= 100)
    {
        // Ενημέρωση οθόνης κάθε 1 δευτερόλεπτο
        calculateRPM();
        updateDisplay();
        engine.lastDisplayTime = currentLoopTime; // Αποθήκευση χρόνου τελευταίας ενημέρωσης
    }
}