// File: include/LCD_tasks.h
#ifndef LCD_TASKS_H
#define LCD_TASKS_H

#include "Kiln_tasks.h"
#include <TFT_eSPI.h> // Include the TFT library for LCD tasks
#include <TFT_eWidget.h> // For graphing

#define TFT_BG 0x2945
#define TFT_MIDNGREEN 0x6884
#define TFT_LIGHTSEAGREEN 0xa2a1
#define TFT_SKBL 0xe311
#define TFT_WATER 0xf5c7

#define HIGHLIGHT           tft.setTextColor(TFT_WATER, TFT_BG);
#define ENDHIGHLIGHT        tft.setTextColor(TFT_SKBL, TFT_BG);

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
uint16_t *getCurrentTempValue();

void drawStartScreen();
void handleStartScreenInput();
void drawEnterStepsScreen();
void handleEnterStepsInput();
void drawEnterPointsForGraph();
void handleEnterPointsForGraphInput();
void highlightCurrentField();
void drawLoadGraph();
void handleLoadGraph();
void graphdraw();
void graphhandle();

void LCD_task(void *pvParameters);

#endif // LCD_TASKS_H