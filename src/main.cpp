#include <Arduino.h>
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

// 1. Variables 
volatile unsigned long lastSparkTime = 0;
int sparkPin = 25; // Pin connected to the spark signal
volatile unsigned long RpmDivide = 0;
const unsigned long debounceDelay = 3000; //microseconds aka 3 millisecond
volatile unsigned long lastDisplayTime = 0;
volatile unsigned long RPM = 0;

//2. Backround 
void IRAM_ATTR recordPulse() {
  unsigned long currentTime;
  unsigned long timeDiff; //IRAM_ATTR gia na ginei to ISR pio grigora(apo8hkeush sthn RAM)
currentTime = micros();
timeDiff = currentTime - lastSparkTime;

if (timeDiff > debounceDelay) {
  RpmDivide = timeDiff;
  lastSparkTime = currentTime; 
}
}

// 3. Initial Setup
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
}//attachInterrupt: sunagermos/ISR


// 4. Main Loop
void loop() {
 if (millis() - lastDisplayTime > 100) { 
  noInterrupts();
  if (micros() - lastSparkTime > 1000000) { //an perase 1 deuterolepto apo ton teleutaio spinthira, tote to RPM einai 0
    RPM = 0;
  } else if (RpmDivide > 0) {
    RPM = 60000000 / RpmDivide; }
    else RPM = 0;
  interrupts();

tft.fillRect(10, 70, 100, 30, TFT_BLACK); // Σβήσε το παλιό νούμερο
tft.setCursor(10, 70);
tft.print("RPM: ");
tft.print(RPM);

  Serial.printf("RPM: %lu\n", RPM);
  lastDisplayTime = millis();
} 
 }

