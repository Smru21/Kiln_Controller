// File: include/LCD_tasks.h
#ifndef LCD_TASKS_H
#define LCD_TASKS_H

#include "Kiln_tasks.h"
#include <TFT_eSPI.h> // Include the TFT library for LCD tasks

extern TFT_eSPI tft; // Invoke custom library

void LCD_setup();

void LCD_startscreen(void *pvParameters);

#endif // LCD_TASKS_H