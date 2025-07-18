// src/IR_tasks.cpp
#include "IR_tasks.h"
#include <irmp.hpp> // Include IRMP library
// #include <freertos/FreeRTOS.h>
// #include <freertos/queue.h>

// Only define irmp_data here, NOT in the header!
IRMP_DATA irmp_data;

TaskHandle_t IRMP_loop_handle; // Task handle for the IRMP loop task

void IR_setup()
{
    // Initialize IRMP library
    irmp_init();

    // Set up the input pin for receiving IR signals
    pinMode(IRMP_INPUT_PIN, INPUT);

    // Initialize serial debugging
    Serial.print(F("Ready to receive IR signals of protocols: "));
    irmp_print_active_protocols(&Serial);
    Serial.println(F("at pin " STR(IRMP_INPUT_PIN)));

    // Create a queue to handle IRMP commands
    // This queue will hold IR commands as uint16_t values
    irmp_queue = xQueueCreate(10, sizeof(uint16_t));

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

    while (irmp_queue == NULL)
    {
        // Wait for the queue to be created
        Serial.println("Waiting for IRMP queue to be created...");
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    // Infinite loop for the task
    for (;;)
    {
        // Check if new IR data is available
        if (irmp_get_data(&irmp_data))
        {
            // Process the IR data
            uint16_t command = irmp_data.command;
            // Send data to queue if there is no repetition
            if (!(irmp_data.flags == IRMP_FLAG_REPETITION))
            {
                // Send the command to the queue
                // This will allow other tasks to process the IR command
                // The queue is used to decouple the IR reception from processing
                // This is useful in a FreeRTOS environment
                xQueueSend(irmp_queue, &command, 0);
            }

            // Print IR command details for DEBUG_MODE
            #if defined(DEBUG_MODE)
            xSemaphoreTake(serialMutex, portMAX_DELAY); // Take the semaphore to ensure exclusive access to serial
            Serial.print("IR Command Received: ");
            Serial.print("Protocol: ");
            Serial.print(irmp_data.protocol);
            Serial.print(" Address: 0x");
            Serial.print(irmp_data.address, HEX);
            Serial.print(" Command: 0x");
            Serial.print(irmp_data.command, HEX);
            Serial.print(" Flags: 0x");
            Serial.println(irmp_data.flags, HEX);
            xSemaphoreGive(serialMutex); // Release the semaphore after printing
            #endif

        }

        // Use proper task delay instead of tight loop
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(10));
    }
}
