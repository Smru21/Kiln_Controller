#ifndef LCD_TASKS_H
#define LCD_TASKS_H

#include "Kiln_tasks.h"
#include <TFT_eSPI.h>
#include <TFT_eWidget.h>

// ─── Color Palette (vibrant, high contrast) ────────────────
#define COL_BG          0x1926   // Deep navy background
#define COL_PANEL       0x2945   // Slightly lighter panel
#define COL_TITLE       0xFFFF   // Bright white titles
#define COL_TEXT         0xCE79   // Soft white body text
#define COL_ACCENT      0x07FF   // Cyan highlights
#define COL_HEAT        0xFD20   // Orange — HEATING
#define COL_HOLD        0xFFE0   // Yellow — HOLDING
#define COL_DONE_COL    0x07E0   // Green — COMPLETE
#define COL_BAR_BG      0x3186   // Dark gray bar background
#define COL_SELECTED    0xF81F   // Magenta selected field
#define COL_DIM         0x7BCF   // Dim gray subtle text
#define COL_MENU_NUM    0xFD20   // Orange for menu numbers [1][2][3]
#define COL_GRID        0x2A69   // Subtle green grid lines
#define COL_TARGET_LINE 0x07FF   // Cyan target trace
#define COL_REAL_LINE   0x07E0   // Green real trace

// ─── Screen States ─────────────────────────────────────────
#define STATE_HOME          0
#define STATE_CONFIRM       1
#define STATE_ENTER_STEPS   2
#define STATE_ENTER_POINTS  3
#define STATE_FIRING        4
#define STATE_DONE          5

// ─── Firing Sub-views ──────────────────────────────────────
#define VIEW_BIG_TEMP   0
#define VIEW_GRAPH      1

// ─── Auto-toggle interval ──────────────────────────────────
#define VIEW_TOGGLE_MS  10000

// ─── Temperature History ───────────────────────────────────
#define MAX_TEMP_HISTORY 900

// ─── TFT instance ──────────────────────────────────────────
extern TFT_eSPI tft;

// ─── Queue helper ──────────────────────────────────────────
template <typename T>
void emptyQueue(QueueHandle_t queue)
{
    T dummy;
    while (xQueueReceive(queue, &dummy, 0) == pdTRUE) { }
}

// ─── Setup & Main Task ────────────────────────────────────
void LCD_setup();
void LCD_task(void *pvParameters);

// ─── Screen Draw Functions ────────────────────────────────
void drawHome();
void drawConfirm();
void drawEnterSteps();
void drawEnterPoints();
void drawFiringBigTemp();
void drawFiringGraph();
void drawDone();

// ─── Screen Input Handlers ────────────────────────────────
void handleHomeInput();
void handleConfirmInput();
void handleEnterStepsInput();
void handleEnterPointsInput();
void handleFiringInput();
void handleDoneInput();

// ─── UI Helpers ───────────────────────────────────────────
void drawProgressBar(uint16_t x, uint16_t y, uint16_t w,
                     uint16_t h, float percent, uint16_t color);
void drawCenteredText(const char *text, int y,
                      uint8_t size, uint16_t color);
void drawMenuOption(const char *key, const char *label,
                    int y, bool selected);
const char *getPhaseString();
uint16_t    getPhaseColor();
uint8_t     getCurrentStepIndex();
float       getFiringPercent();

void updateFiringBigTemp();
void updateFiringGraph();

#endif