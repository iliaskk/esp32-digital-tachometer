#include <Arduino.h>
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

// Variables 
volatile unsigned long lastSparkTime = 0;
const uint8_t sparkPin = 25; // Pin connected to the spark signal
volatile unsigned long timeBetweenSparks = 0;
const unsigned long debounceDelay = 3000; // 3000 microseconds = 3 milliseconds
unsigned long lastDisplayTime = 0;
unsigned long RPM = 0;
unsigned long lastRpmState = 9999; //Impossible Initial State

// ISR Backround 
void IRAM_ATTR recordPulse() {
    unsigned long currentIsrTime = micros();
    unsigned long sparkNew = currentIsrTime - lastSparkTime;

    if (sparkNew > debounceDelay) {
        timeBetweenSparks = sparkNew;
        lastSparkTime = currentIsrTime;
    }
}

void calculateRPM() { 
   noInterrupts();
    if (micros() - lastSparkTime > 1000000) {  // 1 second
      RPM = 0;  // engine stopped
    } else if (timeBetweenSparks > 0) {
      RPM = 60000000 / timeBetweenSparks;  // 60,000,000 microseconds = 1 minute
    } else {
      RPM = 0;
    }
    interrupts();
}

// Display UI; Change-Detection State for Anti-flickering
 void updateDisplay() { 
   if (RPM != lastRpmState) { //an einai to idio me to proigoumeno, den xreiazetai na graftei sthn o8onh
      tft.setCursor(10, 40);
      tft.print("RPM: ");
      tft.print(RPM);
      tft.print("   "); //(spaces) λειτουργούν σαν «έξυπνη γόμα», σβήνοντας μόνο τα ψηφία που περισσεύουν δεξιά, χωρίς να αναβοσβήνουν όλη την περιοχή.
      Serial.printf("RPM: %lu\n", RPM);
      lastRpmState = RPM;
    }
 }

//Initial Setup
void setup() {
  tft.init();
  tft.setRotation(1); 
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setTextSize(2);
  Serial.begin(115200);
  
  tft.setCursor(10, 10);
  tft.println("System Ready!");
  
  pinMode(sparkPin, INPUT_PULLUP); 
  attachInterrupt(digitalPinToInterrupt(sparkPin), recordPulse , RISING); //Σύνδεσε έναν συναγερμό στο Pin 25. Μόλις δεις την τάση να ανεβαίνει (σπινθήρας), τρέξε τη συνάρτηση recordPulse για να αυξήσεις τον μετρητή
} //attachInterrupt: sunagermos/ISR

void loop() { // main loop
  unsigned long currentLoopTime = millis(); //Δεν βάζουμε ΠΟΤΕ την ίδια μεταβλητή να διαβάζει micros() (εκατομμυριοστά) και μετά από λίγο millis()
 if (currentLoopTime - lastDisplayTime >= 100) { //Ενημέρωση οθόνης κάθε 1 δευτερόλεπτο
  calculateRPM(); //Υπολογισμός RPM
  updateDisplay(); //Ενημέρωση οθόνης
  lastDisplayTime = currentLoopTime; //Αποθήκευση χρόνου τελευταίας ενημέρωσης
  }
} 