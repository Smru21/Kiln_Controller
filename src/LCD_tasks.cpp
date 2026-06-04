#include "LCD_tasks.h"

// ─── TFT & Graph Widgets ──────────────────────────────────
TFT_eSPI tft = TFT_eSPI();

GraphWidget  gr           = GraphWidget(&tft);
TraceWidget  target_trace = TraceWidget(&gr);
TraceWidget  real_trace   = TraceWidget(&gr);

// ─── Preset Data ──────────────────────────────────────────
#define BISQUE_STEPS 5
uint16_t bisque_reachTime[BISQUE_STEPS]  = {5,  60, 160, 90,  40};
uint16_t bisque_targetTemp[BISQUE_STEPS] = {100,200, 600, 900, 1000};
uint16_t bisque_holdTime[BISQUE_STEPS]   = {60, 10,  10,  15,  10};

#define GLAZE_STEPS 6
uint16_t glaze_reachTime[GLAZE_STEPS]  = {5,  90, 160, 120, 80,  24};
uint16_t glaze_targetTemp[GLAZE_STEPS] = {100,200, 600, 900,1050,1180};
uint16_t glaze_holdTime[GLAZE_STEPS]   = {15, 10,   5,   5,  15,   5};

// ─── Navigation State ─────────────────────────────────────
volatile uint8_t screen_state      = STATE_HOME;
volatile uint8_t prev_screen_state = 0xFF;
volatile uint8_t firing_view       = VIEW_BIG_TEMP;
volatile uint8_t prev_firing_view  = 0xFF;
uint32_t         last_view_toggle  = 0;

// ─── Firing Profile Arrays ────────────────────────────────
uint8_t  number_of_steps     = 0;
uint16_t *reachTime          = NULL;
uint16_t *targetTemp         = NULL;
uint16_t *holdTime           = NULL;
static bool arrays_are_allocated = false;

// ─── Timing ───────────────────────────────────────────────
uint32_t startTime       = 0;
uint32_t lastTime        = 0;
uint16_t xtime           = 0;
uint16_t totalFireTime   = 0;

// ─── Custom Entry State ───────────────────────────────────
uint8_t  custom_steps_value  = 3;     // UP/DOWN to pick 1–10
uint8_t  entry_step          = 0;     // Which step being edited
uint8_t  entry_field         = 0;     // 0=reach, 1=target, 2=hold
uint16_t entry_values[3]     = {0, 0, 0}; // reach, target, hold for current step

// ─── Preset Selection ─────────────────────────────────────
const char *selected_preset_name = "";

// ─── Temperature History ──────────────────────────────────
float    temp_history[MAX_TEMP_HISTORY];
uint16_t temp_history_count    = 0;
uint32_t last_temp_record_time = 0;

// ─── Firing State ─────────────────────────────────────────
bool firing_complete = false;

// ─── Task Handles ─────────────────────────────────────────
TaskHandle_t      LCD_task_handle = NULL;
SemaphoreHandle_t display_mutex   = NULL;

// ─── Button Input ─────────────────────────────────────────
static char current_command = 'X';

// ═══════════════════════════════════════════════════════════
//  UI HELPERS
// ═══════════════════════════════════════════════════════════

void drawCenteredText(const char *text, int y, uint8_t size, uint16_t color)
{
    tft.setTextSize(size);
    tft.setTextColor(color, COL_BG);
    int16_t x = (320 - tft.textWidth(text)) / 2;
    tft.setCursor(x, y);
    tft.print(text);
}

void drawMenuOption(const char *key, const char *label, int y, bool selected)
{
    uint16_t bg = selected ? COL_PANEL : COL_BG;

    tft.fillRect(40, y - 2, 240, 24, bg);
    tft.setTextSize(2);

    // Draw key like [1]
    tft.setCursor(60, y);
    tft.setTextColor(COL_MENU_NUM, bg);
    tft.print("[");
    tft.print(key);
    tft.print("]");

    // Draw label
    tft.setCursor(110, y);
    tft.setTextColor(selected ? COL_ACCENT : COL_TEXT, bg);
    tft.print(label);
}

void drawProgressBar(uint16_t x, uint16_t y, uint16_t w,
                     uint16_t h, float percent, uint16_t color)
{
    if (percent < 0.0) percent = 0.0;
    if (percent > 100.0) percent = 100.0;

    tft.fillRoundRect(x, y, w, h, h / 2, COL_BAR_BG);

    uint16_t fillW = (uint16_t)((w - 4) * percent / 100.0);
    if (fillW > 0)
        tft.fillRoundRect(x + 2, y + 2, fillW, h - 4, (h - 4) / 2, color);

    char buf[6];
    snprintf(buf, sizeof(buf), "%d%%", (int)percent);
    tft.setTextSize(1);
    tft.setTextColor(COL_TITLE);  // white, no background — visible on both dark gray and fill color
    int16_t tx = x + (w - tft.textWidth(buf)) / 2;
    int16_t ty = y + (h - 8) / 2;
    tft.setCursor(tx, ty);
    tft.print(buf);
}

uint8_t getCurrentStepIndex()
{
    if (startTime == 0 || number_of_steps == 0) return 0;
    uint32_t currTime = getElapsedMinutes();  // ← CHANGED
   
    uint32_t accTime  = 0;
    for (uint8_t i = 0; i < number_of_steps; i++)
    {
        accTime += reachTime[i] + holdTime[i];
        if (currTime <= accTime) return i;
    }
    return number_of_steps - 1;
}

const char *getPhaseString()
{
    if (startTime == 0 || number_of_steps == 0) return "IDLE";
    if (firing_complete) return "COMPLETE";
    uint32_t currTime = getElapsedMinutes();  // ← CHANGED

    
    uint32_t accTime  = 0;
    for (uint8_t i = 0; i < number_of_steps; i++)
    {
        accTime += reachTime[i];
        if (currTime <= accTime) return "HEATING";
        accTime += holdTime[i];
        if (currTime <= accTime) return "HOLDING";
    }
    return "COMPLETE";
}

uint16_t getPhaseColor()
{
    const char *phase = getPhaseString();
    if (strcmp(phase, "HEATING") == 0)  return COL_HEAT;
    if (strcmp(phase, "HOLDING") == 0)  return COL_HOLD;
    if (strcmp(phase, "COMPLETE") == 0) return COL_DONE_COL;
    return COL_TEXT;
}

float getFiringPercent()
{
    if (startTime == 0 || totalFireTime == 0) return 0.0;
    uint32_t elapsed = getElapsedMinutes();   // ← CHANGED
    float pct = (float)elapsed / (float)totalFireTime * 100.0;
    return (pct > 100.0) ? 100.0 : pct;
}

// ═══════════════════════════════════════════════════════════
//  SETUP
// ═══════════════════════════════════════════════════════════

void LCD_setup()
{
    display_mutex = xSemaphoreCreateMutex();
    tft.init();
    tft.setRotation(1);
    pinMode(TFT_BL, OUTPUT);
    analogWrite(TFT_BL, 160);    // Brighter for kids
    tft.fillScreen(COL_BG);

    // Boot splash
    drawCenteredText("KILN MAESTRO", 80, 3, COL_ACCENT);
    drawCenteredText("Starting...", 130, 2, COL_DIM);
    vTaskDelay(pdMS_TO_TICKS(800));

    xTaskCreatePinnedToCore(
        LCD_task, "LCD_task",
        4096, NULL, 1,
        &LCD_task_handle, ARDUINO_RUNNING_CORE
    );
}

// ═══════════════════════════════════════════════════════════
//  MAIN STATE MACHINE
// ═══════════════════════════════════════════════════════════

void LCD_task(void *pvParameters)
{
    TickType_t xLastWakeTime;

    for (;;)
    {
        xLastWakeTime = xTaskGetTickCount();

        switch (screen_state)
        {
        // ── HOME ──────────────────────────────────────────
        case STATE_HOME:
            if (prev_screen_state != STATE_HOME)
            {
                drawHome();
                prev_screen_state = STATE_HOME;
            }
            handleHomeInput();
            vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(50));
            break;

        // ── CONFIRM ───────────────────────────────────────
        case STATE_CONFIRM:
            if (prev_screen_state != STATE_CONFIRM)
            {
                drawConfirm();
                prev_screen_state = STATE_CONFIRM;
            }
            handleConfirmInput();
            vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(50));
            break;

        // ── ENTER STEPS (custom) ──────────────────────────
        case STATE_ENTER_STEPS:
            if (prev_screen_state != STATE_ENTER_STEPS)
            {
                drawEnterSteps();
                prev_screen_state = STATE_ENTER_STEPS;
            }
            handleEnterStepsInput();
            vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(50));
            break;

        // ── ENTER POINTS (custom) ─────────────────────────
        case STATE_ENTER_POINTS:
            if (prev_screen_state != STATE_ENTER_POINTS)
            {
                drawEnterPoints();
                prev_screen_state = STATE_ENTER_POINTS;
            }
            handleEnterPointsInput();
            vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(50));
            break;

        // ── FIRING ────────────────────────────────────────
        // ── FIRING ────────────────────────────────────────
        case STATE_FIRING:
            if (prev_screen_state != STATE_FIRING)
            {
                prev_screen_state = STATE_FIRING;
                prev_firing_view  = 0xFF;
                last_view_toggle  = millis();
            }
            handleFiringInput();

            // Auto-toggle every 10s
            if ((millis() - last_view_toggle) >= VIEW_TOGGLE_MS)
            {
                firing_view = (firing_view == VIEW_BIG_TEMP)
                              ? VIEW_GRAPH : VIEW_BIG_TEMP;
                prev_firing_view = 0xFF;
                last_view_toggle = millis();
            }

            // Draw or update current view
            if (prev_firing_view != firing_view)
            {
                // Full redraw on view switch
                if (firing_view == VIEW_BIG_TEMP)
                    drawFiringBigTemp();
                else
                    drawFiringGraph();
                prev_firing_view = firing_view;
            }
            else
            {
                // Partial update only — no flicker
                if (firing_view == VIEW_BIG_TEMP)
                    updateFiringBigTemp();
                else
                    updateFiringGraph();
            }

            // Record temp history
            if (startTime > 0)
            {
                uint32_t simMinutes = getElapsedMinutes();

                // Record when a new minute has passed (simulated)
                if (simMinutes > xtime && temp_history_count < MAX_TEMP_HISTORY)
                {
                    xSemaphoreTake(temp_mutex, portMAX_DELAY);
                    float t = current_temperature;
                    xSemaphoreGive(temp_mutex);

                    // Fill any skipped minutes (can happen at high speed)
                    while (xtime < simMinutes && temp_history_count < MAX_TEMP_HISTORY)
                    {
                        temp_history[temp_history_count] = t;
                        temp_history_count++;
                        xtime++;
                    }

                    // Redraw graph when new data arrives
                    if (firing_view == VIEW_GRAPH)
                        drawFiringGraph();
                }

                // Check complete
                if (xtime >= totalFireTime && !firing_complete)
                {
                    firing_complete = true;
                    digitalWrite(RELAY_OUT, LOW);
                    vTaskSuspend(Kiln_relay_handle);
                    screen_state = STATE_DONE;
                }
            }

            vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(1000));
            break;

        // ── DONE ──────────────────────────────────────────
        case STATE_DONE:
            if (prev_screen_state != STATE_DONE)
            {
                drawDone();
                prev_screen_state = STATE_DONE;
            }
            handleDoneInput();
            vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(50));
            break;

        default:
            screen_state = STATE_HOME;
            break;
        }
    }
}

// ═══════════════════════════════════════════════════════════
//  SCREEN 0 — HOME
// ═══════════════════════════════════════════════════════════

void drawHome()
{
    emptyQueue<char>(button_queue);
    tft.fillScreen(COL_BG);

    // ── Title bar ─────────────────────────────────────────
    tft.fillRect(0, 0, 320, 40, COL_PANEL);
    tft.fillRect(0, 38, 320, 2, COL_ACCENT);
    drawCenteredText("KILN MAESTRO", 10, 3, COL_ACCENT);

    // ── Menu options ──────────────────────────────────────
    drawMenuOption("1", "BISQUE",  65, false);
    drawMenuOption("2", "GLAZE",   95, false);
    drawMenuOption("3", "CUSTOM", 125, false);

    // ── Separator line ────────────────────────────────────
    tft.drawFastHLine(40, 160, 240, COL_PANEL);

    // ── Current temperature readout ───────────────────────
    xSemaphoreTake(temp_mutex, portMAX_DELAY);
    float temp = current_temperature;
    xSemaphoreGive(temp_mutex);

    tft.setTextSize(1);
    tft.setTextColor(COL_DIM, COL_BG);
    tft.setCursor(90, 175);
    tft.print("Current Temp:");

    char buf[16];
    snprintf(buf, sizeof(buf), "%d C", (int)temp);
    tft.setTextSize(3);
    tft.setTextColor(COL_TITLE, COL_BG);
    int16_t tx = (320 - tft.textWidth(buf)) / 2;
    tft.setCursor(tx, 195);
    tft.print(buf);

    // ── Bottom hint ───────────────────────────────────────
    tft.setTextSize(1);
    tft.setTextColor(COL_DIM, COL_BG);
    tft.setCursor((320 - tft.textWidth("Press 1, 2 or 3 to select")) / 2, 230);
    tft.print("Press 1, 2 or 3 to select");
}

void handleHomeInput()
{
    if (xQueueReceive(button_queue, &current_command,
                      10 * portTICK_PERIOD_MS) == pdTRUE)
    {
        switch (current_command)
        {
        case '1':
            selected_preset_name = "BISQUE";
            number_of_steps = BISQUE_STEPS;
            reachTime  = bisque_reachTime;
            targetTemp = bisque_targetTemp;
            holdTime   = bisque_holdTime;
            arrays_are_allocated = false;
            screen_state = STATE_CONFIRM;
            break;

        case '2':
            selected_preset_name = "GLAZE";
            number_of_steps = GLAZE_STEPS;
            reachTime  = glaze_reachTime;
            targetTemp = glaze_targetTemp;
            holdTime   = glaze_holdTime;
            arrays_are_allocated = false;
            screen_state = STATE_CONFIRM;
            break;

        case '3':
            selected_preset_name = "CUSTOM";
            custom_steps_value = 3;
            screen_state = STATE_ENTER_STEPS;
            break;

        default:
            // Flash feedback for invalid press
            tft.fillRect(40, 230, 240, 10, COL_BG);
            tft.setTextSize(1);
            tft.setTextColor(COL_HEAT, COL_BG);
            tft.setCursor((320 - tft.textWidth("Press 1, 2 or 3!")) / 2, 230);
            tft.print("Press 1, 2 or 3!");
            break;
        }
    }
}

// ═══════════════════════════════════════════════════════════
//  SCREEN 1 — CONFIRM
// ═══════════════════════════════════════════════════════════

void drawConfirm()
{
    emptyQueue<char>(button_queue);
    tft.fillScreen(COL_BG);

    // ── Title bar ─────────────────────────────────────────
    tft.fillRect(0, 0, 320, 36, COL_PANEL);
    tft.fillRect(0, 34, 320, 2, COL_ACCENT);

    char title[32];
    snprintf(title, sizeof(title), "%s FIRING", selected_preset_name);
    drawCenteredText(title, 8, 2, COL_ACCENT);

    // ── Calculate summary ─────────────────────────────────
    uint16_t maxTemp = 0;
    uint16_t totalMin = 0;
    for (uint8_t i = 0; i < number_of_steps; i++)
    {
        if (targetTemp[i] > maxTemp) maxTemp = targetTemp[i];
        totalMin += reachTime[i] + holdTime[i];
    }
    uint16_t hours = totalMin / 60;
    uint16_t mins  = totalMin % 60;

    // ── Info cards ────────────────────────────────────────
    // Steps
    tft.fillRoundRect(20, 50, 280, 30, 6, COL_PANEL);
    tft.setTextSize(2);
    tft.setTextColor(COL_DIM, COL_PANEL);
    tft.setCursor(30, 56);
    tft.print("Steps:");
    tft.setTextColor(COL_TITLE, COL_PANEL);
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", number_of_steps);
    tft.setCursor(280 - tft.textWidth(buf), 56);
    tft.print(buf);

    // Max Temp
    tft.fillRoundRect(20, 88, 280, 30, 6, COL_PANEL);
    tft.setTextColor(COL_DIM, COL_PANEL);
    tft.setCursor(30, 94);
    tft.print("Max Temp:");
    snprintf(buf, sizeof(buf), "%d C", maxTemp);
    tft.setTextColor(COL_HEAT, COL_PANEL);
    tft.setCursor(280 - tft.textWidth(buf), 94);
    tft.print(buf);

    // Duration
    tft.fillRoundRect(20, 126, 280, 30, 6, COL_PANEL);
    tft.setTextColor(COL_DIM, COL_PANEL);
    tft.setCursor(30, 132);
    tft.print("Duration:");
    snprintf(buf, sizeof(buf), "%dh %dm", hours, mins);
    tft.setTextColor(COL_ACCENT, COL_PANEL);
    tft.setCursor(280 - tft.textWidth(buf), 132);
    tft.print(buf);

    // ── Action buttons ────────────────────────────────────
    // START button
    tft.fillRoundRect(20, 175, 135, 40, 8, COL_DONE_COL);
    tft.setTextSize(2);
    tft.setTextColor(COL_BG, COL_DONE_COL);
    tft.setCursor(20 + (135 - tft.textWidth("[OK] START")) / 2, 185);
    tft.print("[OK] START");

    // BACK button
    tft.fillRoundRect(165, 175, 135, 40, 8, COL_BAR_BG);
    tft.setTextColor(COL_TEXT, COL_BAR_BG);
    tft.setCursor(165 + (135 - tft.textWidth("[B] BACK")) / 2, 185);
    tft.print("[B] BACK");

    // ── Bottom hint ───────────────────────────────────────
    tft.setTextSize(1);
    tft.setTextColor(COL_DIM, COL_BG);
    tft.setCursor((320 - tft.textWidth("OK to begin firing, B to go back")) / 2, 228);
    tft.print("OK to begin firing, B to go back");
}

void handleConfirmInput()
{
    if (xQueueReceive(button_queue, &current_command,
                      10 * portTICK_PERIOD_MS) == pdTRUE)
    {
        if (current_command == 'O')
        {
            // ── Start Firing ──────────────────────────────
            totalFireTime = 0;
            for (uint8_t i = 0; i < number_of_steps; i++)
                totalFireTime += reachTime[i] + holdTime[i];

            startTime             = millis();
            lastTime              = startTime;
            xtime                 = 0;
            temp_history_count    = 0;
            last_temp_record_time = millis();
            firing_complete       = false;
            firing_view           = VIEW_BIG_TEMP;

            vTaskResume(Kiln_relay_handle);

            // Quick "starting" feedback
            tft.fillScreen(COL_BG);
            drawCenteredText("STARTING...", 100, 3, COL_DONE_COL);
            vTaskDelay(pdMS_TO_TICKS(600));

            screen_state = STATE_FIRING;
        }
        else if (current_command == 'B')
        {
            // Free custom arrays if going back
            if (arrays_are_allocated)
            {
                free(reachTime);  reachTime  = NULL;
                free(targetTemp); targetTemp = NULL;
                free(holdTime);   holdTime   = NULL;
                arrays_are_allocated = false;
            }
            screen_state = STATE_HOME;
        }
    }
}

// ═══════════════════════════════════════════════════════════
//  SCREEN 2 — ENTER STEPS (custom only)
// ═══════════════════════════════════════════════════════════

void drawEnterSteps()
{
    emptyQueue<char>(button_queue);
    tft.fillScreen(COL_BG);

    // ── Title bar ─────────────────────────────────────────
    tft.fillRect(0, 0, 320, 36, COL_PANEL);
    tft.fillRect(0, 34, 320, 2, COL_ACCENT);
    drawCenteredText("CUSTOM FIRING", 8, 2, COL_ACCENT);

    // ── Prompt ────────────────────────────────────────────
    drawCenteredText("How many steps?", 55, 2, COL_TEXT);

    // ── UP arrow ──────────────────────────────────────────
    tft.fillTriangle(160, 80, 145, 100, 175, 100, COL_ACCENT);

    // ── Number box ────────────────────────────────────────
    tft.fillRoundRect(120, 108, 80, 50, 10, COL_PANEL);
    tft.drawRoundRect(120, 108, 80, 50, 10, COL_ACCENT);

    char buf[4];
    snprintf(buf, sizeof(buf), "%d", custom_steps_value);
    tft.setTextSize(4);
    tft.setTextColor(COL_TITLE, COL_PANEL);
    tft.setCursor(120 + (80 - tft.textWidth(buf)) / 2,
                  108 + (50 - 28) / 2);
    tft.print(buf);

    // ── DOWN arrow ────────────────────────────────────────
    tft.fillTriangle(160, 178, 145, 165, 175, 165, COL_ACCENT);

    // ── Range hint ────────────────────────────────────────
    tft.setTextSize(1);
    tft.setTextColor(COL_DIM, COL_BG);
    tft.setCursor((320 - tft.textWidth("Range: 1 - 10")) / 2, 188);
    tft.print("Range: 1 - 10");

    // ── Action buttons ────────────────────────────────────
    tft.fillRoundRect(20, 205, 135, 30, 6, COL_DONE_COL);
    tft.setTextSize(2);
    tft.setTextColor(COL_BG, COL_DONE_COL);
    tft.setCursor(20 + (135 - tft.textWidth("[OK] NEXT")) / 2, 211);
    tft.print("[OK] NEXT");

    tft.fillRoundRect(165, 205, 135, 30, 6, COL_BAR_BG);
    tft.setTextColor(COL_TEXT, COL_BAR_BG);
    tft.setCursor(165 + (135 - tft.textWidth("[B] BACK")) / 2, 211);
    tft.print("[B] BACK");
}

static void redrawStepsNumber()
{
    tft.fillRoundRect(122, 110, 76, 46, 8, COL_PANEL);

    char buf[4];
    snprintf(buf, sizeof(buf), "%d", custom_steps_value);
    tft.setTextSize(4);
    tft.setTextColor(COL_TITLE, COL_PANEL);
    tft.setCursor(120 + (80 - tft.textWidth(buf)) / 2,
                  108 + (50 - 28) / 2);
    tft.print(buf);
}

void handleEnterStepsInput()
{
    if (xQueueReceive(button_queue, &current_command,
                      10 * portTICK_PERIOD_MS) == pdTRUE)
    {
        if (current_command == 'U')
        {
            if (custom_steps_value < 10) custom_steps_value++;
            redrawStepsNumber();
        }
        else if (current_command == 'D')
        {
            if (custom_steps_value > 1) custom_steps_value--;
            redrawStepsNumber();
        }
        else if (current_command == 'O')
        {
            number_of_steps = custom_steps_value;

            // Allocate arrays
            if (arrays_are_allocated)
            {
                free(reachTime);
                free(targetTemp);
                free(holdTime);
            }
            reachTime  = (uint16_t *)calloc(number_of_steps, sizeof(uint16_t));
            targetTemp = (uint16_t *)calloc(number_of_steps, sizeof(uint16_t));
            holdTime   = (uint16_t *)calloc(number_of_steps, sizeof(uint16_t));

            if (!reachTime || !targetTemp || !holdTime)
            {
                if (reachTime)  free(reachTime);
                if (targetTemp) free(targetTemp);
                if (holdTime)   free(holdTime);
                reachTime  = NULL;
                targetTemp = NULL;
                holdTime   = NULL;
                arrays_are_allocated = false;

                tft.fillRect(20, 188, 280, 12, COL_BG);
                tft.setTextSize(1);
                tft.setTextColor(COL_HEAT, COL_BG);
                tft.setCursor((320 - tft.textWidth("Memory error! Try fewer steps.")) / 2, 188);
                tft.print("Memory error! Try fewer steps.");
                return;
            }

            arrays_are_allocated = true;

            // Set sensible defaults for each step
            for (uint8_t i = 0; i < number_of_steps; i++)
            {
                reachTime[i]  = 60;
                targetTemp[i] = 200 + (i * 150);
                holdTime[i]   = 10;
            }

            entry_step  = 0;
            entry_field = 0;
            entry_values[0] = reachTime[0];
            entry_values[1] = targetTemp[0];
            entry_values[2] = holdTime[0];

            // Feedback
            tft.fillScreen(COL_BG);
            char msg[32];
            snprintf(msg, sizeof(msg), "%d steps created!", number_of_steps);
            drawCenteredText(msg, 100, 2, COL_DONE_COL);
            vTaskDelay(pdMS_TO_TICKS(500));

            screen_state = STATE_ENTER_POINTS;
        }
        else if (current_command == 'B')
        {
            screen_state = STATE_HOME;
        }
    }
}

// ═══════════════════════════════════════════════════════════
//  SCREEN 3 — ENTER POINTS (custom only)
// ═══════════════════════════════════════════════════════════

static const char *field_labels[] = {"Reach:", "Target:", "Hold:"};
static const char *field_units[]  = {"min", "C", "min"};
static const uint16_t field_max[] = {9999, 1250, 9999};

static void drawPointsField(uint8_t field, bool selected)
{
    uint16_t y = 68 + (field * 44);
    uint16_t bg   = selected ? COL_PANEL : COL_BG;
    uint16_t valC = selected ? COL_TITLE : COL_TEXT;

    // Field background
    tft.fillRoundRect(15, y, 290, 38, 6, bg);

    // Selection indicator
    if (selected)
    {
        tft.fillRoundRect(15, y, 5, 38, 2, COL_SELECTED);

        // UP/DOWN arrows on the right
        tft.fillTriangle(285, y + 5, 278, y + 14, 292, y + 14, COL_ACCENT);
        tft.fillTriangle(285, y + 33, 278, y + 24, 292, y + 24, COL_ACCENT);
    }

    // Label
    tft.setTextSize(2);
    tft.setTextColor(COL_DIM, bg);
    tft.setCursor(25, y + 10);
    tft.print(field_labels[field]);

    // Value
    char buf[16];
    snprintf(buf, sizeof(buf), "%d %s", entry_values[field], field_units[field]);
    tft.setTextColor(valC, bg);
    tft.setCursor(260 - tft.textWidth(buf), y + 10);
    tft.print(buf);
}

static void drawAllFields()
{
    for (uint8_t i = 0; i < 3; i++)
        drawPointsField(i, (i == entry_field));
}

void drawEnterPoints()
{
    emptyQueue<char>(button_queue);
    tft.fillScreen(COL_BG);

    // ── Title bar with step info ──────────────────────────
    tft.fillRect(0, 0, 320, 36, COL_PANEL);
    tft.fillRect(0, 34, 320, 2, COL_ACCENT);

    char title[32];
    snprintf(title, sizeof(title), "STEP %d OF %d",
             entry_step + 1, number_of_steps);
    drawCenteredText(title, 8, 2, COL_ACCENT);

    // ── Step progress dots ────────────────────────────────
    uint16_t dotStartX = (320 - (number_of_steps * 16)) / 2;
    for (uint8_t i = 0; i < number_of_steps; i++)
    {
        uint16_t dx = dotStartX + (i * 16);
        if (i < entry_step)
            tft.fillCircle(dx + 6, 50, 5, COL_DONE_COL);   // completed
        else if (i == entry_step)
            tft.fillCircle(dx + 6, 50, 5, COL_ACCENT);      // current
        else
            tft.drawCircle(dx + 6, 50, 5, COL_DIM);         // upcoming
    }

    // ── Entry fields ──────────────────────────────────────
    entry_values[0] = reachTime[entry_step];
    entry_values[1] = targetTemp[entry_step];
    entry_values[2] = holdTime[entry_step];
    drawAllFields();

    // ── Bottom controls ───────────────────────────────────
    tft.fillRect(0, 204, 320, 36, COL_PANEL);

    tft.setTextSize(1);
    tft.setTextColor(COL_ACCENT, COL_PANEL);
    tft.setCursor(8, 208);
    tft.print("U/D: ");
    tft.setTextColor(COL_TEXT, COL_PANEL);
    tft.print("+/-10");

    tft.setTextColor(COL_ACCENT, COL_PANEL);
    tft.setCursor(100, 208);
    tft.print("*/N: ");
    tft.setTextColor(COL_TEXT, COL_PANEL);
    tft.print("+/-100");

    tft.setTextColor(COL_ACCENT, COL_PANEL);
    tft.setCursor(210, 208);
    tft.print("OK: ");
    tft.setTextColor(COL_TEXT, COL_PANEL);
    tft.print("Next");

    tft.setTextColor(COL_ACCENT, COL_PANEL);
    tft.setCursor(270, 208);
    tft.print("B: ");
    tft.setTextColor(COL_TEXT, COL_PANEL);
    tft.print("Prev");

    // Second hint row
    tft.setTextColor(COL_DIM, COL_PANEL);
    tft.setCursor(8, 222);
    tft.print("1-9: Set field directly    #: Select field");
}

static void saveCurrentEntryToArrays()
{
    reachTime[entry_step]  = entry_values[0];
    targetTemp[entry_step] = entry_values[1];
    holdTime[entry_step]   = entry_values[2];
}

static void adjustValue(int16_t delta)
{
    int32_t newVal = (int32_t)entry_values[entry_field] + delta;
    if (newVal < 0)    newVal = 0;
    if (newVal > (int32_t)field_max[entry_field])
        newVal = field_max[entry_field];
    entry_values[entry_field] = (uint16_t)newVal;
    drawPointsField(entry_field, true);
}

static void loadStepIntoEntry(uint8_t step)
{
    entry_step = step;
    entry_values[0] = reachTime[entry_step];
    entry_values[1] = targetTemp[entry_step];
    entry_values[2] = holdTime[entry_step];
    entry_field = 0;

    // Force full redraw
    prev_screen_state = 0xFF;
}

void handleEnterPointsInput()
{
    if (xQueueReceive(button_queue, &current_command,
                      10 * portTICK_PERIOD_MS) == pdTRUE)
    {
        switch (current_command)
        {
        // ── Increment ±10 ─────────────────────────────────
        case 'U':
            adjustValue(+10);
            break;

        case 'D':
            adjustValue(-10);
            break;

        // ── Increment ±100 ────────────────────────────────
        case 'N':
            adjustValue(+100);
            break;

        case '*':
            adjustValue(-100);
            break;

        // ── Cycle active field ────────────────────────────
        case '#':
        {
            uint8_t old_field = entry_field;
            entry_field = (entry_field + 1) % 3;
            drawPointsField(old_field, false);
            drawPointsField(entry_field, true);
            break;
        }

        // ── Quick set via number keys ─────────────────────
        case '1': case '2': case '3': case '4': case '5':
        case '6': case '7': case '8': case '9':
        {
            // Multiply digit × 100 for quick setting
            // e.g., press 6 on target = 600°C
            uint16_t quickVal = (current_command - '0') * 100;
            if (quickVal > field_max[entry_field])
                quickVal = field_max[entry_field];
            entry_values[entry_field] = quickVal;
            drawPointsField(entry_field, true);
            break;
        }

        case '0':
            entry_values[entry_field] = 0;
            drawPointsField(entry_field, true);
            break;

        // ── OK: Next field → next step → confirm ─────────
        case 'O':
        {
            saveCurrentEntryToArrays();

            if (entry_field < 2)
            {
                // Move to next field
                uint8_t old_field = entry_field;
                entry_field++;
                drawPointsField(old_field, false);
                drawPointsField(entry_field, true);
            }
            else if (entry_step < number_of_steps - 1)
            {
                // Move to next step
                loadStepIntoEntry(entry_step + 1);
            }
            else
            {
                // All done — go to confirm
                selected_preset_name = "CUSTOM";
                tft.fillScreen(COL_BG);
                drawCenteredText("All steps set!", 100, 2, COL_DONE_COL);
                vTaskDelay(pdMS_TO_TICKS(500));
                screen_state = STATE_CONFIRM;
            }
            break;
        }

        // ── BACK: Prev field → prev step → back ──────────
        case 'B':
        {
            saveCurrentEntryToArrays();

            if (entry_field > 0)
            {
                // Move to previous field
                uint8_t old_field = entry_field;
                entry_field--;
                drawPointsField(old_field, false);
                drawPointsField(entry_field, true);
            }
            else if (entry_step > 0)
            {
                // Move to previous step
                loadStepIntoEntry(entry_step - 1);
                entry_field = 2;
            }
            else
            {
                // Back to steps screen
                if (arrays_are_allocated)
                {
                    free(reachTime);  reachTime  = NULL;
                    free(targetTemp); targetTemp = NULL;
                    free(holdTime);   holdTime   = NULL;
                    arrays_are_allocated = false;
                }
                screen_state = STATE_ENTER_STEPS;
            }
            break;
        }

        default:
            break;
        }
    }
}

// ═══════════════════════════════════════════════════════════
//  SCREEN 4A — FIRING: BIG TEMP VIEW
// ═══════════════════════════════════════════════════════════

void drawFiringBigTemp()
{
    tft.fillScreen(COL_BG);

    // ── Top status bar ────────────────────────────────────
    uint16_t phaseColor = getPhaseColor();
    const char *phaseStr = getPhaseString();
    uint8_t stepIdx = getCurrentStepIndex();

    tft.fillRect(0, 0, 320, 32, COL_PANEL);
    tft.fillRect(0, 30, 320, 2, phaseColor);

    // Step info left
    tft.setTextSize(2);
    tft.setTextColor(COL_TEXT, COL_PANEL);
    char stepBuf[16];
    snprintf(stepBuf, sizeof(stepBuf), "Step %d/%d", stepIdx + 1, number_of_steps);
    tft.setCursor(8, 8);
    tft.print(stepBuf);

    // Phase right
    tft.setTextColor(phaseColor, COL_PANEL);
    tft.setCursor(320 - tft.textWidth(phaseStr) - 8, 8);
    tft.print(phaseStr);

    // ── Current temperature (BIG) ─────────────────────────
    xSemaphoreTake(temp_mutex, portMAX_DELAY);
    float temp = current_temperature;
    xSemaphoreGive(temp_mutex);

    char tempBuf[16];
    snprintf(tempBuf, sizeof(tempBuf), "%d", (int)temp);

    // Temperature number
    tft.setTextSize(1);
    tft.setTextColor(COL_DIM, COL_BG);
    tft.setCursor((320 - tft.textWidth("CURRENT TEMP")) / 2, 42);
    tft.print("CURRENT TEMP");

    // Big temperature with degree symbol
    tft.setTextSize(7);
    tft.setTextColor(COL_TITLE, COL_BG);
    char bigBuf[16];
    snprintf(bigBuf, sizeof(bigBuf), "%dC", (int)temp);
    int16_t bx = (320 - tft.textWidth(bigBuf)) / 2;
    tft.setCursor(bx, 56);
    tft.print(bigBuf);

    // ── Target temperature ────────────────────────────────
    uint32_t tgt = getTargetTemp();
    char tgtBuf[24];
    snprintf(tgtBuf, sizeof(tgtBuf), "-> %d C", (int)tgt);
    tft.setTextSize(2);
    tft.setTextColor(phaseColor, COL_BG);
    tft.setCursor((320 - tft.textWidth(tgtBuf)) / 2, 118);
    tft.print(tgtBuf);

    // ── Progress bar ──────────────────────────────────────
    float pct = getFiringPercent();
    drawProgressBar(20, 148, 280, 22, pct, phaseColor);

    // ── Time info ─────────────────────────────────────────
    uint32_t elapsedMin = getElapsedMinutes();
    uint32_t remainingMin = (totalFireTime > elapsedMin)
                            ? totalFireTime - elapsedMin : 0;

    tft.fillRect(0, 180, 320, 36, COL_PANEL);

    // Elapsed left
    tft.setTextSize(1);
    tft.setTextColor(COL_DIM, COL_PANEL);
    tft.setCursor(20, 182);
    tft.print("ELAPSED");

    tft.setTextSize(2);
    tft.setTextColor(COL_ACCENT, COL_PANEL);
    char eBuf[16];
    snprintf(eBuf, sizeof(eBuf), "%dh %dm", elapsedMin / 60, elapsedMin % 60);
    tft.setCursor(20, 196);
    tft.print(eBuf);

    // Remaining right
    tft.setTextSize(1);
    tft.setTextColor(COL_DIM, COL_PANEL);
    tft.setCursor(320 - tft.textWidth("REMAINING") - 20, 182);
    tft.print("REMAINING");

    tft.setTextSize(2);
    tft.setTextColor(COL_HEAT, COL_PANEL);
    char rBuf[16];
    snprintf(rBuf, sizeof(rBuf), "%dh %dm", remainingMin / 60, remainingMin % 60);
    tft.setCursor(320 - tft.textWidth(rBuf) - 20, 196);
    tft.print(rBuf);

    // ── Bottom hint ───────────────────────────────────────
    tft.setTextSize(1);
    tft.setTextColor(COL_DIM, COL_BG);
    tft.setCursor((320 - tft.textWidth("Press any button for graph view")) / 2, 228);
    tft.print("Press any button for graph view");
}

// ═══════════════════════════════════════════════════════════
//  SCREEN 4B — FIRING: GRAPH VIEW
// ═══════════════════════════════════════════════════════════

void drawFiringGraph()
{
    tft.fillScreen(COL_BG);

    uint16_t phaseColor = getPhaseColor();
    const char *phaseStr = getPhaseString();
    uint8_t stepIdx = getCurrentStepIndex();

    // ─── Top info bar (y: 0–17) ─────────────────────────
    tft.fillRect(0, 0, 320, 18, COL_PANEL);

    xSemaphoreTake(temp_mutex, portMAX_DELAY);
    float temp = current_temperature;
    xSemaphoreGive(temp_mutex);

    uint32_t tgt = getTargetTemp();

    tft.setTextSize(1);
    tft.setTextColor(COL_TITLE, COL_PANEL);
    tft.setCursor(4, 5);
    char infoBuf[48];
    snprintf(infoBuf, sizeof(infoBuf), "%dC->%dC  S%d/%d",
             (int)temp, (int)tgt, stepIdx + 1, number_of_steps);
    tft.print(infoBuf);

    tft.setTextColor(phaseColor, COL_PANEL);
    tft.setCursor(320 - tft.textWidth(phaseStr) - 4, 5);
    tft.print(phaseStr);

    // ─── Compute axis scales ────────────────────────────
    float graphXMax = (float)totalFireTime;
    if (graphXMax < 10.0) graphXMax = 10.0;

    float graphYMax = 0;
    for (uint8_t i = 0; i < number_of_steps; i++)
    {
        if (targetTemp[i] > graphYMax)
            graphYMax = (float)targetTemp[i];
    }
    graphYMax = graphYMax * 1.15;
    if (graphYMax < 100.0) graphYMax = 100.0;

    if (graphYMax <= 500)       graphYMax = ceil(graphYMax / 50.0)  * 50.0;
    else if (graphYMax <= 1000) graphYMax = ceil(graphYMax / 100.0) * 100.0;
    else                        graphYMax = ceil(graphYMax / 200.0) * 200.0;

    // ─── Layout constants ───────────────────────────────
    // Y-axis labels: ~4 digits max (e.g. "1200") at size 1 = ~24px wide
    // We need ~2px padding from screen edge
    // X-axis labels: ~8px tall at size 1, plus 2px gap
    //
    // Screen regions:
    //   Top bar:       y 0–17   (18px)
    //   Graph zone:    y 18–223 (206px available)
    //   Bottom bar:    y 224–239 (16px)
    //
    // Graph placement:
    //   Y-label area:  x 0–25   (26px for up to 4-digit numbers)
    //   Graph left:    x 26
    //   Graph right:   x 317    (3px margin from edge)
    //   Graph width:   317 - 26 = 291px
    //
    //   Graph top:     y 19     (1px below header)
    //   X-label area:  10px (8px text + 2px gap)
    //   Graph bottom:  y 223 - 10 = y 213
    //   Graph height:  213 - 19 = 194px

    const int gx = 26;        // graph left x
    const int gy = 19;        // graph top y
    const int gw = 291;       // graph width
    const int gh = 194;       // graph height

    gr.createGraph(gw, gh, COL_BG);
    gr.setGraphScale(0.0, graphXMax, 0.0, graphYMax);

    float xGridStep = graphXMax / 6.0;
    float yGridStep = graphYMax / 5.0;
    gr.setGraphGrid(0.0, xGridStep, 0.0, yGridStep, COL_GRID);
    gr.drawGraph(gx, gy);

    // ─── X-axis labels ──────────────────────────────────
    tft.setTextSize(1);
    tft.setTextColor(COL_DIM, COL_BG);
    tft.setTextDatum(TC_DATUM);
    for (int i = 0; i <= 6; i++)
    {
        float xVal = i * xGridStep;
        int px = gr.getPointX(xVal);
        int py = gr.getPointY(0.0) + 2;
        tft.drawNumber((int)xVal, px, py);
    }

    // ─── Y-axis labels ──────────────────────────────────
    tft.setTextDatum(MR_DATUM);
    for (int i = 1; i <= 5; i++)
    {
        float yVal = i * yGridStep;
        int px = gr.getPointX(0.0) - 2;
        int py = gr.getPointY(yVal);
        tft.drawNumber((int)yVal, px, py);
    }
    tft.setTextDatum(TL_DATUM);

    // ─── Target trace ───────────────────────────────────
    target_trace.startTrace(COL_TARGET_LINE);
    if (number_of_steps > 0)
    {
        float traceTime = 0;

        traceTime += reachTime[0];
        target_trace.addPoint(0.0, 0.0);
        target_trace.addPoint(traceTime, (float)targetTemp[0]);

        traceTime += holdTime[0];
        target_trace.addPoint(traceTime, (float)targetTemp[0]);

        for (int i = 1; i < number_of_steps; i++)
        {
            traceTime += reachTime[i];
            target_trace.addPoint(traceTime, (float)targetTemp[i]);

            traceTime += holdTime[i];
            target_trace.addPoint(traceTime, (float)targetTemp[i]);
        }
    }

    // ─── Real trace + marker ────────────────────────────
    if (temp_history_count > 0)
    {
        real_trace.startTrace(COL_REAL_LINE);
        for (uint16_t i = 0; i < temp_history_count; i++)
        {
            real_trace.addPoint((float)i, temp_history[i]);
        }

        float currMin = (float)(temp_history_count - 1);
        if (currMin >= 0)
        {
            int mx = gr.getPointX(currMin);
            int myTop = gr.getPointY(graphYMax);
            int myBot = gr.getPointY(0.0);
            tft.drawFastVLine(mx, myTop, myBot - myTop, COL_REAL_LINE);

            int my = gr.getPointY(temp);
            tft.fillCircle(mx, my, 3, COL_TITLE);
        }
    }

    // ─── Bottom status bar ──────────────────────────────
    uint32_t elapsedMin   = getElapsedMinutes();
    uint32_t remainingMin = (totalFireTime > elapsedMin)
                            ? totalFireTime - elapsedMin : 0;
    float pct = getFiringPercent();

    tft.fillRect(0, 224, 320, 2, COL_BAR_BG);
    uint16_t barW = (uint16_t)(320.0 * pct / 100.0);
    if (barW > 0) tft.fillRect(0, 224, barW, 2, phaseColor);

    tft.fillRect(0, 226, 320, 14, COL_PANEL);
    tft.setTextSize(1);
    tft.setTextColor(COL_ACCENT, COL_PANEL);
    char timeBuf[48];
    snprintf(timeBuf, sizeof(timeBuf), "El: %dh%dm  Rm: %dh%dm  [%d%%]",
             elapsedMin / 60, elapsedMin % 60,
             remainingMin / 60, remainingMin % 60,
             (int)pct);
    tft.setCursor((320 - tft.textWidth(timeBuf)) / 2, 229);
    tft.print(timeBuf);
}


// ═══════════════════════════════════════════════════════════
//  PARTIAL UPDATES (no full screen clear — prevents flicker)
// ═══════════════════════════════════════════════════════════

void updateFiringBigTemp()
{
    uint16_t phaseColor = getPhaseColor();
    const char *phaseStr = getPhaseString();
    uint8_t stepIdx = getCurrentStepIndex();

    // ── Update step/phase in top bar ──────────────────────
    tft.fillRect(0, 0, 320, 30, COL_PANEL);
    tft.setTextSize(2);
    tft.setTextColor(COL_TEXT, COL_PANEL);
    char stepBuf[16];
    snprintf(stepBuf, sizeof(stepBuf), "Step %d/%d", stepIdx + 1, number_of_steps);
    tft.setCursor(8, 8);
    tft.print(stepBuf);

    tft.setTextColor(phaseColor, COL_PANEL);
    tft.setCursor(320 - tft.textWidth(phaseStr) - 8, 8);
    tft.print(phaseStr);

    // Accent line color matches phase
    tft.fillRect(0, 30, 320, 2, phaseColor);

    // ── Update big temperature ────────────────────────────
    xSemaphoreTake(temp_mutex, portMAX_DELAY);
    float temp = current_temperature;
    xSemaphoreGive(temp_mutex);

    tft.fillRect(0, 54, 320, 56, COL_BG);
    tft.setTextSize(7);
    tft.setTextColor(COL_TITLE, COL_BG);
    char bigBuf[16];
    snprintf(bigBuf, sizeof(bigBuf), "%dC", (int)temp);
    tft.setCursor((320 - tft.textWidth(bigBuf)) / 2, 56);
    tft.print(bigBuf);

    // ── Update target ─────────────────────────────────────
    uint32_t tgt = getTargetTemp();
    tft.fillRect(0, 116, 320, 20, COL_BG);
    char tgtBuf[24];
    snprintf(tgtBuf, sizeof(tgtBuf), "-> %d C", (int)tgt);
    tft.setTextSize(2);
    tft.setTextColor(phaseColor, COL_BG);
    tft.setCursor((320 - tft.textWidth(tgtBuf)) / 2, 118);
    tft.print(tgtBuf);

    // ── Update progress bar ───────────────────────────────
    float pct = getFiringPercent();
    drawProgressBar(20, 148, 280, 22, pct, phaseColor);

    // ── Update time ───────────────────────────────────────
    uint32_t elapsedMin = getElapsedMinutes();
    uint32_t remainingMin = (totalFireTime > elapsedMin)
                            ? totalFireTime - elapsedMin : 0;

    tft.fillRect(0, 180, 320, 36, COL_PANEL);

    tft.setTextSize(1);
    tft.setTextColor(COL_DIM, COL_PANEL);
    tft.setCursor(20, 182);
    tft.print("ELAPSED");
    tft.setTextSize(2);
    tft.setTextColor(COL_ACCENT, COL_PANEL);
    char eBuf[16];
    snprintf(eBuf, sizeof(eBuf), "%dh %dm", elapsedMin / 60, elapsedMin % 60);
    tft.setCursor(20, 196);
    tft.print(eBuf);

    tft.setTextSize(1);
    tft.setTextColor(COL_DIM, COL_PANEL);
    tft.setCursor(320 - tft.textWidth("REMAINING") - 20, 182);
    tft.print("REMAINING");
    tft.setTextSize(2);
    tft.setTextColor(COL_HEAT, COL_PANEL);
    char rBuf[16];
    snprintf(rBuf, sizeof(rBuf), "%dh %dm", remainingMin / 60, remainingMin % 60);
    tft.setCursor(320 - tft.textWidth(rBuf) - 20, 196);
    tft.print(rBuf);
}

void updateFiringGraph()
{
    // ── Update compact info bar only ──────────────────────
    uint16_t phaseColor = getPhaseColor();
    const char *phaseStr = getPhaseString();
    uint8_t stepIdx = getCurrentStepIndex();

    tft.fillRect(0, 0, 320, 18, COL_PANEL);

    xSemaphoreTake(temp_mutex, portMAX_DELAY);
    float temp = current_temperature;
    xSemaphoreGive(temp_mutex);

    uint32_t tgt = getTargetTemp();

    tft.setTextSize(1);
    tft.setTextColor(COL_TITLE, COL_PANEL);
    tft.setCursor(4, 5);
    char infoBuf[48];
    snprintf(infoBuf, sizeof(infoBuf), "%dC->%dC  S%d/%d",
             (int)temp, (int)tgt, stepIdx + 1, number_of_steps);
    tft.print(infoBuf);

    tft.setTextColor(phaseColor, COL_PANEL);
    tft.setCursor(320 - tft.textWidth(phaseStr) - 4, 5);
    tft.print(phaseStr);

    // ── Update bottom time bar ────────────────────────────
    uint32_t elapsedMin = getElapsedMinutes();
    uint32_t remainingMin = (totalFireTime > elapsedMin)
                            ? totalFireTime - elapsedMin : 0;
    float pct = getFiringPercent();

    tft.fillRect(0, 222, 320, 2, COL_BAR_BG);
    uint16_t barW = (uint16_t)(320.0 * pct / 100.0);
    if (barW > 0) tft.fillRect(0, 222, barW, 2, phaseColor);

    tft.fillRect(0, 224, 320, 16, COL_PANEL);
    tft.setTextSize(1);
    tft.setTextColor(COL_ACCENT, COL_PANEL);
    char timeBuf[48];
    snprintf(timeBuf, sizeof(timeBuf), "El: %dh%dm  Rm: %dh%dm  [%d%%]",
             elapsedMin / 60, elapsedMin % 60,
             remainingMin / 60, remainingMin % 60,
             (int)pct);
    tft.setCursor((320 - tft.textWidth(timeBuf)) / 2, 228);
    tft.print(timeBuf);
}
// ═══════════════════════════════════════════════════════════
//  FIRING INPUT HANDLER
// ═══════════════════════════════════════════════════════════

void handleFiringInput()
{
    if (xQueueReceive(button_queue, &current_command, 0) == pdTRUE)
    {
        // Any button toggles between views
        firing_view = (firing_view == VIEW_BIG_TEMP)
                      ? VIEW_GRAPH : VIEW_BIG_TEMP;
        prev_firing_view = 0xFF;   // Force redraw
        last_view_toggle = millis(); // Reset auto-toggle timer
    }
}

// ═══════════════════════════════════════════════════════════
//  SCREEN 5 — DONE
// ═══════════════════════════════════════════════════════════

void drawDone()
{
    emptyQueue<char>(button_queue);
    tft.fillScreen(COL_BG);
    uint32_t totalElapsed = getElapsedMinutes();  // ← CHANGED
    // ── Green accent border ───────────────────────────────
    tft.drawRect(0, 0, 320, 240, COL_DONE_COL);
    tft.drawRect(1, 1, 318, 238, COL_DONE_COL);
    tft.drawRect(2, 2, 316, 236, COL_DONE_COL);

    // ── Checkmark icon ────────────────────────────────────
    // Simple big checkmark using triangles
    int cx = 160, cy = 55;
    tft.fillCircle(cx, cy, 25, COL_DONE_COL);
    // Draw check with lines
    for (int t = -2; t <= 2; t++)
    {
        tft.drawLine(cx - 14, cy + t,     cx - 4,  cy + 10 + t, COL_BG);
        tft.drawLine(cx - 4,  cy + 10 + t, cx + 14, cy - 8 + t, COL_BG);
    }

    // ── Title ─────────────────────────────────────────────
    drawCenteredText("FIRING COMPLETE", 92, 3, COL_DONE_COL);

    // ── Final stats ───────────────────────────────────────
    xSemaphoreTake(temp_mutex, portMAX_DELAY);
    float finalTemp = current_temperature;
    xSemaphoreGive(temp_mutex);

    uint32_t getElapsedMinutes();

    // Temperature card
    tft.fillRoundRect(30, 128, 260, 30, 6, COL_PANEL);
    tft.setTextSize(2);
    tft.setTextColor(COL_DIM, COL_PANEL);
    tft.setCursor(40, 134);
    tft.print("Final Temp:");
    char buf[16];
    snprintf(buf, sizeof(buf), "%d C", (int)finalTemp);
    tft.setTextColor(COL_ACCENT, COL_PANEL);
    tft.setCursor(270 - tft.textWidth(buf), 134);
    tft.print(buf);

    // Time card
    tft.fillRoundRect(30, 164, 260, 30, 6, COL_PANEL);
    tft.setTextColor(COL_DIM, COL_PANEL);
    tft.setCursor(40, 170);
    tft.print("Total Time:");
    snprintf(buf, sizeof(buf), "%dh %dm", totalElapsed / 60, totalElapsed % 60);
    tft.setTextColor(COL_ACCENT, COL_PANEL);
    tft.setCursor(270 - tft.textWidth(buf), 170);
    tft.print(buf);

    // ── OK button ─────────────────────────────────────────
    tft.fillRoundRect(90, 204, 140, 28, 8, COL_DONE_COL);
    tft.setTextSize(2);
    tft.setTextColor(COL_BG, COL_DONE_COL);
    tft.setCursor(90 + (140 - tft.textWidth("[OK] HOME")) / 2, 210);
    tft.print("[OK] HOME");
}

void handleDoneInput()
{
    if (xQueueReceive(button_queue, &current_command,
                      10 * portTICK_PERIOD_MS) == pdTRUE)
    {
        if (current_command == 'O')
        {
            // Clean up
            startTime       = 0;
            firing_complete = false;
            xtime           = 0;
            temp_history_count = 0;

            if (arrays_are_allocated)
            {
                free(reachTime);  reachTime  = NULL;
                free(targetTemp); targetTemp = NULL;
                free(holdTime);   holdTime   = NULL;
                arrays_are_allocated = false;
            }

            screen_state = STATE_HOME;
        }
    }
}