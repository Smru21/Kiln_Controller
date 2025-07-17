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

#include "IR_tasks.h"

void Kiln_tasks_setup();

#endif // KILN_TASKS_H
