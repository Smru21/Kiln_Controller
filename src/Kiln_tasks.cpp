// src/Kiln_tasks.cpp
#include "Kiln_tasks.h"

QueueHandle_t button_queue = NULL; // Initialize the button queue to NULL
QueueHandle_t irmp_queue = NULL; // Initialize the queue to NULL

void Kiln_setup()
{
    button_queue = xQueueCreate(10, sizeof(uint16_t)); // Create a queue for button commands
    xTaskCreatePinnedToCore(
        Kiln_IR_decode,      // Task function
        "Kiln_IR_decode",    // Name of the task
        2048,                // Stack size in bytes
        NULL,                // Task parameters
        1,                   // Priority of the task
        NULL,                // Task handle (not used here)
        ARDUINO_RUNNING_CORE // Core to run the task on
    );
}

void Kiln_IR_decode(void* pvParameters) {
    while (irmp_queue == NULL)
    {
        // Wait for the queue to be created
        Serial.println("Waiting for IRMP queue to be created...");
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    // Infinite loop for the IR decoding task
    for (;;)
    {
        static char button_pressed; // Array for character + null terminator // Variable to store the button pressed
        uint16_t recv_command; // Variable to store the command received
        // Check if new IR data is available
        if (xQueueReceive(irmp_queue, &recv_command, 10 * portTICK_PERIOD_MS) == pdTRUE) {
            // Process the button pressed
            button_pressed = commandToButton(recv_command);
            xQueueSend(button_queue, &button_pressed, 5 * portTICK_PERIOD_MS); // Send the button pressed to the button queue
            serial_debugging_print("Button pressed: ");
            serial_debugging_println(button_pressed); // Print the button pressed to the serial console
        }

        vTaskDelay(pdMS_TO_TICKS(100)); // Delay to avoid busy-waiting
    }
}
