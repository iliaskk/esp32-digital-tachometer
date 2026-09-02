## ESP32 Motorcycle Digital Tachometer

A robust, modular C++ firmware for reading motorcycle engine RPM using an ESP32. Designed specifically for single-cylinder engines, this project captures ignition coil pulses via an optocoupler and displays the real-time RPM on an SPI TFT screen.

### Core Features

* **Hardware Interrupts (ISR):** Utilizes `IRAM_ATTR` hardware interrupts to capture spark pulses with microsecond precision, completely independent of the main loop.
* **Concurrency & Atomicity:** Isolates ISR write operations from main-loop read operations via scoped critical sections (`noInterrupts()` / `interrupts()`) to eliminate race conditions in shared memory.
* **Hardware Rollover Handling:** Addressed the 71-minute 32-bit `micros()` counter wrap-around to prevent persistent lockup states when the engine halts.
* **Flicker-Free Rendering:** Employs a state-based UI update system. The SPI TFT display only redraws when the RPM value physically changes, utilizing overwrite padding instead of screen clearing to eliminate visual flickering.
* **Compiler Hygiene:** Enforces strict code quality via `-Wall` and `-Wextra` build flags, ensuring a zero-warning compilation process.

### Hardware Requirements

* **Microcontroller:** ESP32 Development Board
* **Display:** SPI TFT Display (using the `TFT_eSPI` library)
* **Signal Isolation:** Optocoupler (e.g., PC817 or 4N35) circuit connected to the motorcycle's spark plug wire/ignition coil.
* **Pull-up Resistor:** Internal `INPUT_PULLUP` enabled on the ESP32, though an external resistor is recommended for high-noise automotive environments.

### Software Architecture & Engineering Decisions

The firmware is structured around strict C++ encapsulation and the Single Responsibility Principle:

* **`recordPulse()`:** An ISR that triggers on the `RISING` edge of the optocoupler signal. It records the delta time (`micros()`) between legitimate sparks.
* **`calculateRPM()`:** Safely reads the shared `volatile` variables within a critical section and calculates the RPM based on the microsecond interval between sparks.
* **`updateDisplay()`:** Handles the graphics. Checks if the newly calculated RPM differs from the `lastRpmState` before committing data to the SPI bus.

### Getting Started

**Prerequisites**
This project is built using PlatformIO.

1. Install VS Code and the PlatformIO IDE extension.
2. Clone this repository:

```bash
git clone https://github.com/iliaskk/esp32-digital-tachometer.git
