# ESP32 Motorcycle Digital Tachometer

A robust, modular firmware for reading motorcycle engine RPM using an ESP32. Designed specifically for single-cylinder engines, this project captures ignition coil pulses via an optocoupler and displays the real-time RPM on an SPI TFT screen.

##  Features

*   **Hardware Interrupts (ISR):** Utilizes `IRAM_ATTR` hardware interrupts to capture spark pulses with microsecond precision, completely independent of the main loop.
*   **Signal Debouncing:** Implements a strict software debounce mechanism (3ms threshold) to filter out high-frequency electrical noise and false triggers from the ignition coil.
*   **Flicker-Free Rendering:** Employs a state-based UI update system. The TFT display via SPI only redraws when the RPM value physically changes, utilizing overwrite padding instead of screen clearing to eliminate visual flickering.
*   **Modular Architecture:** Clean separation of concerns between Hardware Capture (`recordPulse`), Business Logic (`calculateRPM`), and Presentation Logic (`updateDisplay`).
*   **Non-Blocking Timers:** Avoids `delay()` entirely. The main loop acts as a deterministic scheduler using `millis()` to coordinate calculation and display tasks without halting the CPU.

##  Hardware Requirements

*   **Microcontroller:** ESP32 Development Board
*   **Display:** SPI TFT Display (using the `TFT_eSPI` library)
*   **Signal Isolation:** Optocoupler (e.g., PC817 or 4N35) circuit connected to the motorcycle's spark plug wire/ignition coil.
*   **Pull-up Resistor:** Internal `INPUT_PULLUP` enabled on the ESP32, though an external resistor is recommended for high-noise automotive environments.

##  Software Architecture

The firmware is structured around the **Single Responsibility Principle**:

1.  **`recordPulse()`:** An ISR that triggers on the `RISING` edge of the optocoupler signal. It records the delta time (`micros()`) between legitimate sparks.
2.  **`calculateRPM()`:** Safely reads the shared volatile variables and calculates the RPM based on the microsecond interval between sparks.
3.  **`updateDisplay()`:** Handles the graphics. Checks if the newly calculated RPM differs from the `lastRpmState` before committing data to the SPI bus.

##  Getting Started

### Prerequisites
This project is built using **PlatformIO**. 

1. Install [VS Code](https://code.visualstudio.com/) and the [PlatformIO IDE extension](https://platformio.org/install/ide?install=vscode).
2. Clone this repository:
   ```bash
   git clone [https://github.com/YOUR_USERNAME/esp32-digital-tachometer.git](https://github.com/YOUR_USERNAME/esp32-digital-tachometer.git)
