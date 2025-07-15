#include "Serial_debugging.h"

void serial_debugging_init()
{
    Serial.begin(115200); // Initialize serial communication at 115200 baud rate
    while (!Serial) {
        ; // Wait for serial port to connect. Needed for native USB devices
    }
    Serial.println("Serial debugging initialized.");
}

void serial_debugging_print(const char* message)
{
    Serial.println(message); // Print the debug message to the serial console
}