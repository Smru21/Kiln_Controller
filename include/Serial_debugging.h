#ifndef SERIAL_DEBUGGING_H
#define SERIAL_DEBUGGING_H

#define DEBUG_MODE
#include <Arduino.h>

/**
 * @brief Initializes serial debugging.
 * 
 * This function sets up the serial communication for debugging purposes.
 * It is typically called in the setup() function of your Arduino sketch.
 */
void serial_debugging_init();

/**
 * @brief Prints a debug message to the serial console.
 * 
 * @param message The message to print.
 */
void serial_debugging_print(const char* message);

#endif // SERIAL_DEBUGGING_H
