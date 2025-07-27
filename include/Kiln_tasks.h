#ifndef KILN_TASKS_H
#define KILN_TASKS_H

/*
 * Kiln_tasks.h
 *
 * This file contains task definitions for the Kiln project.
 * It calls xTaskCreate for each task.
 * The tasks are defined in various source files.
 * Each task is responsible for a specific functionality in the Kiln system.
 * The tasks are created with a priority and stack size suitable for their operation.
 */

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include "remote_buttons.h" // Include remote button definitions
#include "Serial_debugging.h" // Include serial debugging functions

#define TC_INPUT 34
#define RELAY_OUT 32

extern uint32_t current_temperature;
extern uint32_t analogVal;
extern uint32_t startTime;
extern uint32_t lastTime;
extern uint16_t xtime;

extern uint8_t number_of_steps;
extern uint16_t *reachTime;  // Pointer to dynamically allocated array of reachTimes.
extern uint16_t *targetTemp; // Pointer to dynamically allocated array of targetTemps.
extern uint16_t *holdTime;   // Pointer to dynamically allocated array of holdTimes.

// Queue handles
extern QueueHandle_t irmp_queue;   // Queue to handle IRMP commands
extern QueueHandle_t button_queue; // Queue to handle button commands
// Semaphores
extern SemaphoreHandle_t display_mutex; // Mutex for display operations
extern SemaphoreHandle_t ir_mutex;      // Mutex to prevent ir commands piling up while switching windows
extern SemaphoreHandle_t temp_mutex;      // Mutex to read and write temperature
extern SemaphoreHandle_t time_mutex;    // Mutex to set and read time
// Task handles of all tasks
extern TaskHandle_t IRMP_loop_handle; // Task handle for the IRMP loop task

extern TaskHandle_t Kiln_IR_decode_handle; // Task handle for the IR decoding task

extern TaskHandle_t Kiln_relay_handle; // Task to handle relay

extern TaskHandle_t temperatureRead_handle;

extern TaskHandle_t LCD_task_handle; // Task handle for all lcd windows. Use a state machine to switch between windows.

void Kiln_setup();

uint32_t getTargetTemp();

// FreeRTOS kiln central tasks
// This task decodes IR signals and sends button presses to the button queue
void Kiln_IR_decode(void* pvParameters);

void Read_temp_task(void *pvParameters);

void Kiln_relay_task(void *pvParameters);

#endif // KILN_TASKS_H
