// src/Serial_debugging.cpp

#include "Serial_debugging.h"

SemaphoreHandle_t serialMutex = NULL; // Mutex for serial debugging communication

#ifdef DEBUG_MODE
void serial_debugging_init()
{
    Serial.begin(115200); // Initialize serial communication at 115200 baud rate
    while (!Serial) {
        ; // Wait for serial port to connect. Needed for native USB port only
    }
    serialMutex = xSemaphoreCreateMutex(); // Create a binary semaphore for serial debugging
    Serial.println("Serial debugging initialized.");
}

#endif // DEBUG_MODE
