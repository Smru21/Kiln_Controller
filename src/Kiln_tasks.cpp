// src/Kiln_tasks.cpp
#include "Kiln_tasks.h"

QueueHandle_t button_queue = NULL; // Initialize the button queue to NULL

SemaphoreHandle_t ir_mutex = NULL;
SemaphoreHandle_t temp_mutex = NULL;

// Task handles of all internal managing tasks
TaskHandle_t Kiln_IR_decode_handle; // Task handle for the IR decoding task
TaskHandle_t temperatureRead_handle;
TaskHandle_t Kiln_relay_handle; // Task to handle relay
uint32_t current_temperature = 0;
uint32_t analogVal = 0;

uint32_t getTargetTemp()
{
    serial_debugging_println("GURU GANDU");

    // Fix: Calculate elapsed time correctly
    uint32_t currTime = (millis() - startTime) / 60000; // Convert to minutes
    uint32_t idxTime = 0;
    uint32_t target = 0;
    uint8_t idx = 0;
    bool reachorhold = false;

    // Find which segment we're in
    for (idx = 0; idx < number_of_steps && idxTime < currTime; idx++)
    {
        // First check if we're in a reach phase
        if (reachTime[idx] > 0)
        {
            idxTime += reachTime[idx];
            if (idxTime > currTime)
            {
                // We're in this reach phase
                reachorhold = false;
                break;
            }
        }

        // Check if we're in a hold phase
        idxTime += holdTime[idx];
        if (idxTime > currTime)
        {
            // We're in this hold phase
            reachorhold = true;
            break;
        }
    }

    // Bounds check
    if (idx >= number_of_steps)
    {
        // We've exceeded all steps, return last temperature
        serial_debugging_println("Exceeded all steps");
        return targetTemp[number_of_steps - 1];
    }

    if (reachorhold)
    {
        // We're in a hold phase, return the target temperature
        target = targetTemp[idx];
    }
    else
    {
        // We're in a reach phase, calculate interpolated temperature
        uint32_t phaseStartTime = idxTime - reachTime[idx];
        uint32_t timeInPhase = currTime - phaseStartTime;

        if (idx == 0)
        {
            // First reach from 0 to targetTemp[0]
            target = (timeInPhase * targetTemp[idx]) / reachTime[idx];
        }
        else
        {
            // Reach from previous temp to current temp
            uint32_t tempDiff = targetTemp[idx] - targetTemp[idx - 1];
            target = targetTemp[idx - 1] + (timeInPhase * tempDiff) / reachTime[idx];
        }
    }

    serial_debugging_println("GURU DEATH");
    return target;
}

void Kiln_setup()
{
    button_queue = xQueueCreate(5, sizeof(char)); // Create a queue for button commands
    ir_mutex = xSemaphoreCreateMutex();
    temp_mutex = xSemaphoreCreateMutex();
    pinMode(TC_INPUT, INPUT);
    pinMode(RELAY_OUT, OUTPUT);

    xTaskCreatePinnedToCore(
        Kiln_IR_decode,                 // Task function
        "Kiln_IR_decode",               // Name of the task
        1024,                           // Stack size in bytes
        NULL,                           // Task parameters
        1,                              // Priority of the task
        &Kiln_IR_decode_handle,         // Task handle (not used here)
        ARDUINO_RUNNING_CORE            // Core to run the task on
    );

    xTaskCreatePinnedToCore(
        Read_temp_task,         // Task function
        "Temperature reading",       // Name of the task
        1024,                   // Stack size in bytes
        NULL,                   // Task parameters
        1,                      // Priority of the task
        &temperatureRead_handle, // Task handle (not used here)
        ARDUINO_RUNNING_CORE    // Core to run the task on
    );

    xTaskCreatePinnedToCore(
        Kiln_relay_task,     // Task function
        "Kiln_relay_task",   // Name of the task
        2048,                // Stack size in bytes
        NULL,                // Task parameters
        1,                   // Priority of the task
        &Kiln_relay_handle,  // Task to handle relay, // Task handle (not used here)
        ARDUINO_RUNNING_CORE // Core to run the task on
    );

    vTaskSuspend(Kiln_relay_handle);
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

void Read_temp_task(void *pvParameters)
{
    for (;;)
    {
        analogVal = analogReadMilliVolts(TC_INPUT);
        xSemaphoreTake(temp_mutex, portMAX_DELAY);
        if(analogVal < 52)
        {
            current_temperature = 37;
        }
        else if(analogVal > 4500)
        {
            current_temperature = 9999;
        }
        else{
            current_temperature = map(analogVal, 52, 4414, 37, 1230); //in degrees *C
        }

        xSemaphoreGive(temp_mutex);
  
        vTaskDelay(5000 * portTICK_PERIOD_MS);
    }
}

void Kiln_relay_task(void *pvParameters)
{
    for (;;)
    {
        // Check if arrays are allocated
        if (reachTime == NULL || targetTemp == NULL || holdTime == NULL || number_of_steps == 0)
        {
            serial_debugging_println("Arrays not initialized, suspending relay task");
            vTaskSuspend(NULL); // Suspend self
        }

        uint32_t target_temperature = getTargetTemp();

        xSemaphoreTake(temp_mutex, portMAX_DELAY);
        if (current_temperature < target_temperature)
        {
            digitalWrite(RELAY_OUT, HIGH);
            serial_debugging_println("RELAY ON");
        }
        else
        {
            digitalWrite(RELAY_OUT, LOW);
            serial_debugging_println("RELAY OFF");
        }
        xSemaphoreGive(temp_mutex);

        vTaskDelay(5000 * portTICK_PERIOD_MS);
    }
}
