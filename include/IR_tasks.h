// include/IR_tasks.h

#ifndef IR_TASKS_H
#define IR_TASKS_H

#include <Arduino.h>
#include "Serial_debugging.h" // Include serial debugging functions
#include "Kiln_tasks.h" // Include Kiln tasks header which contains common RTOS resources
// #include <freertos/FreeRTOS.h>
// #include <freertos/task.h>
// #include <freertos/queue.h>
/*
 * Set input pin and output pin definitions etc.
 */
#include "Pin_definitions.h"

#define IRMP_PROTOCOL_NAMES 1 // Enable protocol number mapping to protocol strings - requires some FLASH. Must before #include <irmp*>

// #include <irmpSelectMain15Protocols.h>  // This enables 15 main protocols
#define IRMP_SUPPORT_NEC_PROTOCOL 1 // this enables only one protocol
// #define IRMP_SUPPORT_SIRCS_PROTOCOL      1 // this enables only one protocol

/*
 * We use LED_BUILTIN as feedback for commands 0x40 and 0x48 and cannot use it as feedback LED for receiving
 */
#if defined(ALTERNATIVE_IR_FEEDBACK_LED_PIN)
#define IRMP_FEEDBACK_LED_PIN ALTERNATIVE_IR_FEEDBACK_LED_PIN
#endif

void IR_setup(); // doesn't need to be a task. Just initializes the IRMP lib.

void IR_loop(void *pvParameters); // polls the ir receiver

#endif // IR_TASKS_H