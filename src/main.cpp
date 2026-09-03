#include <Arduino.h>
#include <TFT_eSPI.h>
#include <cinttypes>

#include <atomic>  //** */ volatile + noInterrupts() is single-core-only synchronization** fix needed
#include <cstdint> //++

TFT_eSPI tft = TFT_eSPI();

const uint8_t SPARK_PIN = 25; // Pin connected to the spark signal
const uint32_t MICROS_PER_MINUTE = 60'000'000;
const uint32_t ENGINE_TIMEOUT_US = 1'000'000;
const uint32_t INVALID_RPM_STATE = 0xFFFFFFFF;
const uint32_t MAX_RPM = 9500;
const uint32_t displayRefreshRate = 100;
const uint32_t debounceDelay = MICROS_PER_MINUTE / MAX_RPM; // debounceDelay is derived from MAX_RPM. These are unrelated quantities: the debounce window is a property of the input signal (glitch filter), and it silently changes if the redline constant is edited. Expressing a filter as "one period at max RPM" is fragile at the top of the range, combined with the strict >   , /++ RC Low-Pass Filter Implemantation needed
const uint8_t TFT_RPM_X = 10;
const uint8_t TFT_RPM_Y = 40;

portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED; // spinlock for ISR synchronization
// Είναι μια μακροεντολή αρχικοποίησης.portMUX_INITIALIZER_UNLOCKED Λέει στο σύστημα κατά το boot: «Αυτή η κλειδαριά ξεκινάει ανοιχτή (unlocked), μην μπλοκάρεις κανέναν ακόμα».Όταν βάζεις Spinlock και στις δύο πλευρές, όποιος όποιος καλέσει πρώτος το portENTER_CRITICAL κλειδώνει.To tachometer είναι ένα σειριακό πρόγραμμα (loop) που δέχεται τυχαίες, απροειδοποίητες "επισκέψεις" από το hardware (recordPulse).
// Το Spinlock είναι ο μηχανισμός που επιβάλλει τάξη σε αυτό το χάος: αναγκάζει τον ασύγχρονο εισβoλέα και τον σειριακό εκτελεστή να συμπεριφερθoύν σαν να διαβάζoυν γραμμή-γραμμή στo μoναδικό σημειo που αγγίζουν τα ιδια bytes στη RAM.

// Data Encapsulation (Τo "κουτί" μe τiς μεταβλητές που αλλάζουν)
class Tachometer // too many responsibilities?
{

private:                   // Εδώ μπαίνουν οι μεταβλητές που δεν θέλουμε να πειράζει η loop()
    uint32_t currentRpm;   // Είναι το τελικό αποτέλεσμα του μαθηματικού υπολογισμού. Πρέπει να αλλάζει μόνο μέσα από τη μέθοδο calculateRpm().
    uint32_t lastRpmState; // Χρησιμοποιείται αποκλειστικά από την updateDisplay() για Anti-Flickering

    enum class EngineState // προσφέρει Type Safety,Είναι απλώς ένα αυστηρό, πεπερασμένο σύνολο από ετικέτες,αναγκάζει μια μεταβλητή να παίρνει τιμές αποκλειστικά μέσα από αυτό το σύνολο,Σε υποχρεώνει να γράφεις πάντα το όνομά του από μπροστά (π.χ. EngineState::ENGINE_STOPPED), οπότε δεν υπάρχει καμία περίπτωση να μπερδευτεί με κάτι άλλο στο πρόγραμμα
    {
        ENGINE_STOPPED,
        ENGINE_RUNNING
    };

    EngineState currentState = EngineState::ENGINE_STOPPED; // Η επίσημη κατάσταση λειτουργίας του κινητήρα. Αλλάζει αυστηρά βάσει των κανόνων του state machine μέσα στην κλάση, προστατεύοντας το σύστημα από ακούσιες μεταβολές.

public:                                  // Οι μεταβλητές που πρέπει αναγκαστικά να βλέπει το ISR.
    volatile uint32_t timeBetweenSparks; // both are used for calculateRpm(),recordPulse(). volatile Μην την αποθηκεύσεις προσωρινά σε κάποιον γρήγορο CPU register, διότι η τιμή της μπορεί να αλλάξει ανά πάσα στιγμή (για το calculateRpm)
    volatile uint32_t lastSparkTime;     // both are used for calculateRpm(),recordPulse() (which is why a spinlock is needed)
    uint32_t lastDisplayTime;

    //  Constructor
    Tachometer()
    {
        lastSparkTime = 0;
        timeBetweenSparks = 0;
        currentRpm = 0;
        lastDisplayTime = 0;
        lastRpmState = INVALID_RPM_STATE;
    }

    // Οι "Λειτουργίες" (API)
    void calculateRpm()
    {
        // we use local variables so we can run calculateRpm() and then pass the data to the engine struct to achieve State Management
        uint32_t localLastSparkTime = 0;
        uint32_t localTimeBetweenSparks = 0;
        uint32_t localCurrentRpm = 0;
        uint32_t localCurrentTime;

        portENTER_CRITICAL(&timerMux); // software interrupt,& epeidh οι δύο πυρήνες κοιτάζουν και πειράζουν το ίδιο ακριβώς byte στη RAM.

        localCurrentTime = micros(); // an εκτελείται έξω (πριν) από το portENTER_CRITICAL -> window of vulnerability) ανάμεσα στη λήψη του χρόνου και την ανάγνωση των μεταβλητών της ISR. -> race condition
        localLastSparkTime = lastSparkTime;
        localTimeBetweenSparks = timeBetweenSparks;

        portEXIT_CRITICAL(&timerMux);

        if ((localCurrentTime - localLastSparkTime > ENGINE_TIMEOUT_US) || (localTimeBetweenSparks == 0))
        {
            currentState = EngineState::ENGINE_STOPPED; // Engine Stop Detection/State Machine
            localCurrentRpm = 0;
        }
        else
        {
            currentState = EngineState::ENGINE_RUNNING;
            localCurrentRpm = MICROS_PER_MINUTE / localTimeBetweenSparks;

            if (localCurrentRpm > MAX_RPM)
            {
                localCurrentRpm = currentRpm; // reject glitch, keep the last valid RPM
            }
        }

        currentRpm = localCurrentRpm;
    }
    void updateDisplay() // Τι κάνει: Διαβάζει το currentRpm και το συγκρίνει με το lastRpmState. Επιτρέπει το I/O προς το hardware της οθόνης αυστηρά και μόνο όταν τα δεδομένα (state) έχουν υποστεί μετάλλαξη (mutation)
    {
        if (currentRpm != lastRpmState)
        {
            char rpmBuffer[16]; // Προσωρινή μνήμη 16 bytes

            // Φορμάρει το string με σταθερό πλάτος
            snprintf(rpmBuffer, sizeof(rpmBuffer), "RPM: %5" PRIu32, currentRpm);

            tft.setCursor(TFT_RPM_X, TFT_RPM_Y);
            tft.print(rpmBuffer); // Τυπώνει το έτοιμο string, κάνοντας τέλειο overwrite

            Serial.printf("RPM: %" PRIu32 "\n", currentRpm);
            lastRpmState = currentRpm;
        }
    }
};

Tachometer tacho; // object of the Tachometer class

// ISR Background
void IRAM_ATTR recordPulse()
{
    uint32_t localIsrTime = micros();
    uint32_t localSparkNew = localIsrTime - tacho.lastSparkTime;

    if (localSparkNew >= debounceDelay)
    {
        portENTER_CRITICAL_ISR(&timerMux); // mesa sto if gia na mhn kleidwnei  to ka8e hlektriko parasito

        tacho.timeBetweenSparks = localSparkNew;
        tacho.lastSparkTime = localIsrTime;

        portEXIT_CRITICAL_ISR(&timerMux);
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

    pinMode(SPARK_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(SPARK_PIN), recordPulse, RISING); // Σύνδεσε έναν συναγερμό στο Pin 25. Μόλις δεις την τάση να ανεβαίνει (σπινθήρας), τρέξε τη συνάρτηση recordPulse για να αυξήσεις τον μετρητή
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