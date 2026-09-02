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
class tachometer
{

private: // Εδώ μπαίνουν οι μεταβλητές που δεν θέλουμε να πειράζει η loop()
    uint32_t currentRpm;
    uint32_t lastRpmState;

public:                                  // Οι μεταβλητές που πρέπει αναγκαστικά να βλέπει το ISR.
    volatile uint32_t timeBetweenSparks; // αλλάζει τιμή μέσα στο (ISR)
    volatile uint32_t lastSparkTime;     // αλλάζει τιμή μέσα στο (ISR)
    uint32_t lastDisplayTime;

    //  Constructor
    tachometer()
    {
        lastSparkTime = 0;
        timeBetweenSparks = 0;
        currentRpm = 0;
        lastDisplayTime = 0;
        lastRpmState = impossibleInitialRpm;
    }

    // Οι "Λειτουργίες" (API)
    void(calculateRpm)()
    {
        // we use local variables so we can run calculateRpm() and then pass the data to the engine struct to achieve State Management
        uint32_t localLastSparkTime = 0;
        uint32_t localTimeBetweenSparks = 0;
        uint32_t localCurrentRpm = 0;
        uint32_t localCurrentTime = micros();

        noInterrupts(); // software interrupt
        localLastSparkTime = lastSparkTime;
        localTimeBetweenSparks = timeBetweenSparks;
        interrupts();

        if ((localCurrentTime - localLastSparkTime > engineTimeoutLimit) || (localTimeBetweenSparks == 0))
        {

            localCurrentRpm = 0; // engine stopped

            noInterrupts();
            lastSparkTime = localCurrentTime; // Prevents micros() 71-min rollover bug
            interrupts();
        }
        else
        {
            localCurrentRpm = microsPerMinute / localTimeBetweenSparks;
        }

        currentRpm = localCurrentRpm;
    }

    // Display UI; Change-Detection State for Anti-Flickering
    void(updateDisplay)()
    {
        if (currentRpm != lastRpmState) // if rpm has changed, update the display
        {
            tft.setCursor(10, 40);
            tft.print("RPM: ");
            tft.print(currentRpm);
            tft.print("   "); // works like a "smart eraser"
            Serial.printf("RPM: %" PRIu32 "\n", currentRpm);
            lastRpmState = currentRpm;
        }
    }
};

tachometer tacho; // object of the tachometer class

// ISR Background
void IRAM_ATTR recordPulse()
{
    uint32_t localIsrTime = micros();
    uint32_t localSparkNew = localIsrTime - tacho.lastSparkTime;

    if (localSparkNew > debounceDelay)
    {
        tacho.timeBetweenSparks = localSparkNew;
        tacho.lastSparkTime = localIsrTime;
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
} // attachInterrupt; an alarm/HARDWARE interrupt

void loop()
{
    uint32_t localCurrentLoopTime = millis(); // Δεν βάζουμε ΠΟΤΕ την ίδια μεταβλητή να διαβάζει micros() (εκατομμυριοστά) και μετά από λίγο millis()

    if (localCurrentLoopTime - tacho.lastDisplayTime >= displayRefreshRate)
    {
        tacho.calculateRpm();
        tacho.updateDisplay();
        tacho.lastDisplayTime = localCurrentLoopTime; // Αποθήκευση χρόνου τελευταίας ενημέρωσης
    }
}