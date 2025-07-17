#include "IR_tasks.h"
#include <irmp.hpp> // Include IRMP library
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

// Only define irmp_data here, NOT in the header!
IRMP_DATA irmp_data;

TaskHandle_t IRMP_loop_handle; // Task handle for the IRMP loop task

QueueHandle_t irmp_queue; // Queue to handle IRMP data

void IR_setup()
{
    // Initialize IRMP library
    irmp_init();

    // Initialize serial debugging
    serial_debugging_init(); // Initialize serial debugging

    // Set up the input pin for receiving IR signals
    pinMode(IRMP_INPUT_PIN, INPUT);

    // Initialize serial debugging
    Serial.print(F("Ready to receive IR signals of protocols: "));
    irmp_print_active_protocols(&Serial);
    Serial.println(F("at pin " STR(IRMP_INPUT_PIN)));

    // Create a queue to handle IRMP data
    irmp_queue = xQueueCreate(10, sizeof(IRMP_DATA));

    // Create the IRMP loop task
    xTaskCreatePinnedToCore(
        IR_loop,             // Task function
        "IRMP_loop",         // Name of the task
        2048,                // Stack size in bytes
        NULL,                // Task parameters
        1,                   // Priority of the task
        &IRMP_loop_handle,   // Task handle
        ARDUINO_RUNNING_CORE // Core to run the task on
    );
}

void IR_loop(void *pvParameters)
{
    // Add proper initialization for the task
    TickType_t xLastWakeTime;
    xLastWakeTime = xTaskGetTickCount();

    // Infinite loop for the task
    for (;;)
    {
        // Check if new IR data is available
        if (irmp_get_data(&irmp_data))
        {
            // Send data to queue
            xQueueSend(irmp_queue, &irmp_data, 0);

            // Print IR command details
            Serial.print("Protocol: ");
            Serial.print(irmp_data.protocol);
            Serial.print(" Address: 0x");
            Serial.print(irmp_data.address, HEX);
            Serial.print(" Command: 0x");
            Serial.print(irmp_data.command, HEX);
            Serial.print(" Flags: 0x");
            Serial.println(irmp_data.flags, HEX);

            // Optional: Print protocol name if IRMP_PROTOCOL_NAMES is defined
            irmp_print_active_protocols(&Serial);
        }

        // Use proper task delay instead of tight loop
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(10));
    }
}
