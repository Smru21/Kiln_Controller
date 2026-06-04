#include "Kiln_tasks.h"

QueueHandle_t button_queue = NULL;
SemaphoreHandle_t ir_mutex = NULL;
SemaphoreHandle_t temp_mutex = NULL;

TaskHandle_t Kiln_IR_decode_handle;
TaskHandle_t temperatureRead_handle;
TaskHandle_t Kiln_relay_handle;

float current_temperature = 0;
float analogVal = 0;
float voltage = 0;
float emaVoltage = 0;
bool firstReading = true;

static void route_vref_to_gpio()
{
    esp_err_t status = adc_vref_to_gpio(ADC_UNIT_1, GPIO_NUM_33);
    if (status == ESP_OK)
        Serial.println("v_ref routed to GPIO\n");
    else
        Serial.println("failed to route v_ref\n");
}

static void check_efuse(void)
{
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK)
        Serial.println("eFuse Two Point: Supported\n");
    else
        Serial.println("eFuse Two Point: NOT supported\n");

    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_VREF) == ESP_OK)
        Serial.println("eFuse Vref: Supported\n");
    else
        Serial.println("eFuse Vref: NOT supported\n");
}

static void print_char_val_type(esp_adc_cal_value_t val_type)
{
    if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP)
        Serial.println("Characterized using Two Point Value\n");
    else if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF)
        Serial.println("Characterized using eFuse Vref\n");
    else
        Serial.println("Characterized using Default Vref\n");
}

float getEMAVoltage()
{
    uint32_t totalVoltage = 0;

    for (int i = 0; i < NUM_READINGS; i++)
    {
        auto adcReading = adc1_get_raw((adc1_channel_t)channel);
        if (adcReading > 0)
        {
            totalVoltage += esp_adc_cal_raw_to_voltage(adcReading, adcCharacteristics);
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }

    float currentVoltage = (totalVoltage / NUM_READINGS) / 1000.0;

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

uint32_t getElapsedMinutes()
{
    if (startTime == 0) return 0;
#ifdef SIM_MODE
    return (uint32_t)((millis() - startTime) * SIM_SPEED / 60000UL);
#else
    return (uint32_t)((millis() - startTime) / 60000UL);
#endif
}

uint32_t getTargetTemp()
{
    if (reachTime == NULL || targetTemp == NULL ||
        holdTime == NULL || number_of_steps == 0)
        return 0;

    uint32_t currTime = getElapsedMinutes();
    uint32_t accumulatedTime = 0;

    serial_debugging_print("Current time: ");
    serial_debugging_print(currTime);
    serial_debugging_print(" minutes, ");

    for (uint8_t idx = 0; idx < number_of_steps; idx++)
    {
        if (reachTime[idx] > 0)
        {
            uint32_t reachPhaseEnd = accumulatedTime + reachTime[idx];

            if (currTime <= reachPhaseEnd)
            {
                if (idx == 0)
                {
                    serial_debugging_print("In first reach phase, target: ");
                    serial_debugging_println(targetTemp[0]);
                    return targetTemp[0];
                }
                else
                {
                    uint32_t timeInReachPhase = currTime - accumulatedTime;
                    uint32_t startTemp = targetTemp[idx - 1];
                    uint32_t target = startTemp +
                        ((targetTemp[idx] - startTemp) * timeInReachPhase) / reachTime[idx];

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

        uint32_t holdPhaseEnd = accumulatedTime + holdTime[idx];

        if (currTime <= holdPhaseEnd)
        {
            serial_debugging_print("In hold phase ");
            serial_debugging_print(idx);
            serial_debugging_print(", target: ");
            serial_debugging_println(targetTemp[idx]);

            return targetTemp[idx];
        }
        accumulatedTime = holdPhaseEnd;
    }

    serial_debugging_print("Past all steps, target: ");
    serial_debugging_println(targetTemp[number_of_steps - 1]);

    return targetTemp[number_of_steps - 1];
}

void Kiln_setup()
{
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);
    button_queue = xQueueCreate(5, sizeof(char));
    ir_mutex     = xSemaphoreCreateMutex();
    temp_mutex   = xSemaphoreCreateMutex();
    pinMode(RELAY_OUT, OUTPUT);

    check_efuse();

    adc1_config_width(width);
    adc1_config_channel_atten(channel, atten);

    adcCharacteristics = (esp_adc_cal_characteristics_t *)
                          calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(
                          unit, atten, width, DEFAULT_VREF, adcCharacteristics);
    print_char_val_type(val_type);

    Serial.print("vRef: ");
    Serial.println(adcCharacteristics->vref);
    Serial.println("Ready to measure voltage on GPIO34 with EMA filtering");

    xTaskCreatePinnedToCore(
        Kiln_IR_decode, "Kiln_IR_decode",
        2048, NULL, 1,
        &Kiln_IR_decode_handle, ARDUINO_RUNNING_CORE
    );

    xTaskCreatePinnedToCore(
        Read_temp_task, "Temperature reading",
        2048, NULL, 1,
        &temperatureRead_handle, ARDUINO_RUNNING_CORE
    );

    xTaskCreatePinnedToCore(
        Kiln_relay_task, "Kiln_relay_task",
        2048, NULL, 1,
        &Kiln_relay_handle, ARDUINO_RUNNING_CORE
    );

    vTaskSuspend(Kiln_relay_handle);
}

void Kiln_IR_decode(void *pvParameters)
{
    while (irmp_queue == NULL)
    {
        Serial.println("Waiting for IRMP queue to be created...");
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    for (;;)
    {
        static char button_pressed;
        uint16_t recv_command;

        if (xQueueReceive(irmp_queue, &recv_command,
                          10 * portTICK_PERIOD_MS) == pdTRUE)
        {
            button_pressed = commandToButton(recv_command);
            xQueueSend(button_queue, &button_pressed, 5 * portTICK_PERIOD_MS);
            serial_debugging_print("Button pressed: ");
            serial_debugging_println(button_pressed);
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void Read_temp_task(void *pvParameters)
{
    static float prev_voltage = 0;
    static float prev_temp = 0;
    static float extrapolated_temp = 0;
    static bool saturated = false;

    for (;;)
    {
        xSemaphoreTake(temp_mutex, portMAX_DELAY);

#ifdef SIM_MODE
        float target = (float)getTargetTemp();
        float diff = target - current_temperature;

        if (diff > 0)
            current_temperature += diff * 0.15;
        else
            current_temperature += diff * 0.06;

        float noise = ((float)(random(-20, 20))) / 10.0;
        current_temperature += noise;

        if (current_temperature < 20.0) current_temperature = 20.0;
        if (current_temperature > 1300.0) current_temperature = 1300.0;

        serial_debugging_print("SIM TEMP: ");
        serial_debugging_print(current_temperature, 1);
        serial_debugging_print(" -> TARGET: ");
        serial_debugging_println(target, 1);

#elif defined(TEMP_EXTRAPOLATION_HACK)
        // ── Temporary extrapolation above ADC ceiling ─────
        // ⚠️ REMOVE THIS after adding voltage divider
        // The real calibration formula is NOT changed

        float v = getEMAVoltage();
        float raw_temp = (309.0 * v + 38.8);  // PROTECTED formula

        serial_debugging_print("RAW V: ");
        serial_debugging_print(v, 4);

        if (v >= 2.75)
        {
            // ADC is saturating
            if (!saturated)
            {
                // Just entered saturation — save entry point
                saturated = true;
                extrapolated_temp = raw_temp;
                serial_debugging_print(" SATURATED at ");
                serial_debugging_println(raw_temp, 1);
            }

            // Estimate: relay is ON means heating, OFF means cooling
            // Use target temp to guide estimate
            float target = (float)getTargetTemp();
            float diff = target - extrapolated_temp;

            if (diff > 0)
                extrapolated_temp += diff * 0.02;  // Slow approach
            else
                extrapolated_temp += diff * 0.01;  // Even slower cooling

            current_temperature = extrapolated_temp;

            serial_debugging_print("  EXTRAP TEMP: ");
            serial_debugging_println(current_temperature, 1);
        }
        else
        {
            // Normal range — use real reading
            saturated = false;
            current_temperature = raw_temp;
            extrapolated_temp = raw_temp;

            serial_debugging_print("  TEMP: ");
            serial_debugging_println(current_temperature, 1);
        }

        prev_voltage = v;
        prev_temp = raw_temp;

#else
        // PROTECTED — DO NOT CHANGE
        float voltage = getEMAVoltage();
        current_temperature = (309.0 * voltage + 38.8);

        serial_debugging_print("RAW V: ");
        serial_debugging_print(voltage, 4);
        serial_debugging_print("  TEMP: ");
        serial_debugging_println(current_temperature, 1);
#endif

        xSemaphoreGive(temp_mutex);

#ifdef SIM_MODE
        vTaskDelay(pdMS_TO_TICKS(500));
#else
        vTaskDelay(pdMS_TO_TICKS(3000));
#endif
    }
}

void Kiln_relay_task(void *pvParameters)
{
    for (;;)
    {
        uint32_t target_temperature = getTargetTemp();

        xSemaphoreTake(temp_mutex, portMAX_DELAY);
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

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}