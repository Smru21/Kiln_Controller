// src/Kiln_tasks.cpp
#include "Kiln_tasks.h"

QueueHandle_t button_queue = NULL; // Initialize the button queue to NULL

SemaphoreHandle_t ir_mutex = NULL;
SemaphoreHandle_t temp_mutex = NULL;

// Task handles of all internal managing tasks
TaskHandle_t Kiln_IR_decode_handle; // Task handle for the IR decoding task
TaskHandle_t temperatureRead_handle;
TaskHandle_t Kiln_relay_handle; // Task to handle relay
float current_temperature = 0;
float analogVal = 0;

float voltage = 0;
float emaVoltage = 0; // EMA filtered voltage
bool firstReading = true;

static void route_vref_to_gpio()
{
    esp_err_t status = adc_vref_to_gpio(ADC_UNIT_1, GPIO_NUM_33);
    if (status == ESP_OK)
    {
        Serial.println("v_ref routed to GPIO\n");
    }
    else
    {
        Serial.println("failed to route v_ref\n");
    }
}

static void check_efuse(void)
{
    // Check if TP is burned into eFuse
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK)
    {
        Serial.println("eFuse Two Point: Supported\n");
    }
    else
    {
        Serial.println("eFuse Two Point: NOT supported\n");
    }
    // Check Vref is burned into eFuse
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_VREF) == ESP_OK)
    {
        Serial.println("eFuse Vref: Supported\n");
    }
    else
    {
        Serial.println("eFuse Vref: NOT supported\n");
    }
}

static void print_char_val_type(esp_adc_cal_value_t val_type)
{
    if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP)
    {
        Serial.println("Characterized using Two Point Value\n");
    }
    else if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF)
    {
        Serial.println("Characterized using eFuse Vref\n");
    }
    else
    {
        Serial.println("Characterized using Default Vref\n");
    }
}

float getEMAVoltage()
{
    uint32_t totalVoltage = 0;

    // Take multiple readings and average them
    for (int i = 0; i < NUM_READINGS; i++)
    {
        auto adcReading = adc1_get_raw((adc1_channel_t)channel);
        if (adcReading > 0)
        {
            totalVoltage += esp_adc_cal_raw_to_voltage(adcReading, adcCharacteristics);
        }
        vTaskDelay(pdMS_TO_TICKS(5)); // Small delay between readings
    }

    float currentVoltage = (totalVoltage / NUM_READINGS) / 1000.0;

    // Apply EMA filter
    if (firstReading)
    {
        emaVoltage = currentVoltage;
        firstReading = false;
    }
    else
    {
        emaVoltage = (EMA_ALPHA * currentVoltage) + ((1 - EMA_ALPHA) * emaVoltage);
    }

    return emaVoltage;
}

uint32_t getTargetTemp()
{
    // Add null checks
    if (reachTime == NULL || targetTemp == NULL || holdTime == NULL || number_of_steps == 0)
    {
        return 0; // Safe default
    }

    // Calculate elapsed time correctly
    uint32_t currTime = (millis() - startTime) / 60000; // Convert to minutes
    uint32_t accumulatedTime = 0;

    serial_debugging_print("Current time: ");
    serial_debugging_print(currTime);
    serial_debugging_print(" minutes, ");

    // Iterate through each step to find which phase we're in
    for (uint8_t idx = 0; idx < number_of_steps; idx++)
    {
        // Phase 1: Reach phase (ramp to target temperature)
        if (reachTime[idx] > 0)
        {
            uint32_t reachPhaseEnd = accumulatedTime + reachTime[idx];

            if (currTime <= reachPhaseEnd)
            {
                // We're in the reach phase for step idx
                if (idx == 0)
                {
                    // First reach phase: NO interpolation, jump directly to target
                    serial_debugging_print("In first reach phase, target: ");
                    serial_debugging_println(targetTemp[0]);
                    return targetTemp[0];
                }
                else
                {
                    // Subsequent reach phases: DO interpolation from previous temp to current temp
                    uint32_t timeInReachPhase = currTime - accumulatedTime;
                    uint32_t startTemp = targetTemp[idx - 1];

                    // Linear interpolation during reach phase
                    uint32_t target = startTemp + ((targetTemp[idx] - startTemp) * timeInReachPhase) / reachTime[idx];

                    serial_debugging_print("In reach phase ");
                    serial_debugging_print(idx);
                    serial_debugging_print(", interpolating from ");
                    serial_debugging_print(startTemp);
                    serial_debugging_print(" to ");
                    serial_debugging_print(targetTemp[idx]);
                    serial_debugging_print(", target: ");
                    serial_debugging_println(target);

                    return target;
                }
            }
            accumulatedTime = reachPhaseEnd;
        }

        // Phase 2: Hold phase (maintain target temperature)
        uint32_t holdPhaseEnd = accumulatedTime + holdTime[idx];

        if (currTime <= holdPhaseEnd)
        {
            // We're in the hold phase for step idx
            serial_debugging_print("In hold phase ");
            serial_debugging_print(idx);
            serial_debugging_print(", target: ");
            serial_debugging_println(targetTemp[idx]);

            return targetTemp[idx];
        }
        accumulatedTime = holdPhaseEnd;
    }

    // If we've gone past all steps, return the final temperature
    serial_debugging_print("Past all steps, target: ");
    serial_debugging_println(targetTemp[number_of_steps - 1]);

    return targetTemp[number_of_steps - 1];
}

void Kiln_setup()
{
    analogReadResolution(12);                     // Set to 12-bit resolution (0-4095)
    analogSetAttenuation(ADC_11db);               // For 0-3.3V range
    button_queue = xQueueCreate(5, sizeof(char)); // Create a queue for button commands
    ir_mutex = xSemaphoreCreateMutex();
    temp_mutex = xSemaphoreCreateMutex();
    // pinMode(TC_INPUT, INPUT);
    pinMode(RELAY_OUT, OUTPUT);

    check_efuse();

    // Configure ADC
    adc1_config_width(width);
    adc1_config_channel_atten(channel, atten);

    // Characterize ADC
    adcCharacteristics = (esp_adc_cal_characteristics_t *)calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(unit, atten, width, DEFAULT_VREF, adcCharacteristics);
    print_char_val_type(val_type);

    Serial.print("vRef: ");
    Serial.println(adcCharacteristics->vref);

    Serial.println("Ready to measure voltage on GPIO34 with EMA filtering");

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
        // In Read_temp_task():
        xSemaphoreTake(temp_mutex, portMAX_DELAY);
        float voltage = getEMAVoltage();

        current_temperature = (309.0 * voltage + 38.8);

        xSemaphoreGive(temp_mutex);
  
        vTaskDelay(3000 * portTICK_PERIOD_MS);
    }
}

void Kiln_relay_task(void *pvParameters)
{
    for (;;)
    { // Add the infinite loop
        uint32_t target_temperature = getTargetTemp();

        xSemaphoreTake(temp_mutex, portMAX_DELAY); // Protect temperature read
        float current_temp_local = current_temperature;
        xSemaphoreGive(temp_mutex);

        serial_debugging_print("Analog V: ");
        serial_debugging_print(voltage, 4);
        serial_debugging_print(" | Current: ");
        serial_debugging_print(current_temp_local, 1);
        serial_debugging_print(" | Target: ");
        serial_debugging_print(target_temperature);

        if (current_temp_local < target_temperature)
        {
            digitalWrite(RELAY_OUT, HIGH);
            serial_debugging_println(" | RELAY ON");
        }
        else
        {
            digitalWrite(RELAY_OUT, LOW);
            serial_debugging_println(" | RELAY OFF");
        }

        vTaskDelay(pdMS_TO_TICKS(1000)); // Add delay - check every second
    }
}
