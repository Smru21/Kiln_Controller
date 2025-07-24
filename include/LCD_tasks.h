// File: include/LCD_tasks.h
#ifndef LCD_TASKS_H
#define LCD_TASKS_H

#include "Kiln_tasks.h"
#include <TFT_eSPI.h> // Include the TFT library for LCD tasks

#define TASK0 LCD_startscreen_handle
#define TASK1 LCD_startscreen_handle
#define TASK2 LCD_startscreen_handle
#define TASK3 LCD_startscreen_handle
#define TASK4 LCD_startscreen_handle
#define TASK5 LCD_startscreen_handle

extern TFT_eSPI tft; // Invoke custom library

template <typename T>
void emptyQueue(QueueHandle_t queue)
{
    T dummy;
    while (xQueueReceive(queue, &dummy, 0) == pdTRUE)
    {
        // Discard all items
    }
}

void LCD_setup();

int charToNumber(char ch);

void drawStartScreen();
void handleStartScreenInput();
void drawEnterStepsScreen();
void handleEnterStepsInput();

void LCD_task(void *pvParameters);

#endif // LCD_TASKS_H