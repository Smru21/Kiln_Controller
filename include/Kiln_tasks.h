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

extern QueueHandle_t irmp_queue; // Queue to handle IRMP commands
extern QueueHandle_t button_queue; // Queue to handle button commands

void Kiln_setup();

void Kiln_IR_decode(void* pvParameters);

#endif // KILN_TASKS_H
