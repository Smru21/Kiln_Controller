// include/Serial_debugging.h

#ifndef SERIAL_DEBUGGING_H
#define SERIAL_DEBUGGING_H

#define DEBUG_MODE
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#ifdef DEBUG_MODE
extern SemaphoreHandle_t serialMutex; // Mutex for serial debugging communication

/**
 * @brief Initializes serial debugging.
 * 
 * This function sets up the serial communication for debugging purposes.
 * It is typically called in the setup() function of your Arduino sketch.
 */
void serial_debugging_init();

// Template functions - must be defined in header
template <typename T>
void serial_debugging_print(T message)
{
    xSemaphoreTake(serialMutex, portMAX_DELAY);
    Serial.print(message);
    xSemaphoreGive(serialMutex);
}

template <typename T>
void serial_debugging_println(T message)
{
    xSemaphoreTake(serialMutex, portMAX_DELAY);
    Serial.println(message);
    xSemaphoreGive(serialMutex);
}

// Two parameter templates for format specifiers (HEX, DEC, BIN, OCT)
template <typename T>
void serial_debugging_print(T value, int format)
{
    xSemaphoreTake(serialMutex, portMAX_DELAY);
    Serial.print(value, format);
    xSemaphoreGive(serialMutex);
}

template <typename T>
void serial_debugging_println(T value, int format)
{
    xSemaphoreTake(serialMutex, portMAX_DELAY);
    Serial.println(value, format);
    xSemaphoreGive(serialMutex);
}

#endif // DEBUG_MODE

#endif // SERIAL_DEBUGGING_H
