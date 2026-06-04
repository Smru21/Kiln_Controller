<img width="780" height="1040" alt="11" src="https://github.com/user-attachments/assets/a5b19a0d-0ee1-4275-83cd-c3cc8e8ff2a6" />
# 🏺 Kiln Maestro: ESP32-Based Professional Kiln Controller

A feature-rich, child-friendly kiln controller built on the ESP32 using FreeRTOS. This system manages high-temperature firing profiles (Bisque, Glaze, Custom) with real-time graph visualization.

## 🚀 Key Features
- **Multithreaded Architecture:** Powered by FreeRTOS for simultaneous sensing, control, and UI rendering.
- **Dynamic UI:** 2.4" TFT display with alternating Big Temp and Real-time Graph views.
- **Child-Friendly UX:** Simplified IR remote navigation with ±10/±100 increment controls.
- **Safety First:** Includes a software extrapolation layer for ADC saturation and EMA filtering for signal stability.

## 🛠️ Hardware Stack
- **MCU:** ESP32 (NodeMCU-32S)
- **Display:** ILI9341 2.4" TFT
- **Sensor:** K-Type Thermocouple with custom Op-Amp amplification
- **Input:** NEC IR Remote (HX1838)

## 📁 Project Structure
- `/src`: Core logic, FreeRTOS tasks, and UI state machine.
- `/include`: Header definitions and configuration.
- `platformio.ini`: Project dependencies and build flags.

## 📈 Skills Demonstrated
- RTOS Concurrency (Semaphores/Queues)
- Digital Signal Processing (EMA Filtering)
- Embedded GUI Design
- Hardware-Software Integration
