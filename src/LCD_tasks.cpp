// File: src/LCD_tasks.cpp

#include "LCD_tasks.h"

TFT_eSPI tft = TFT_eSPI();

/*
 * 0 - Startscreen(Load or create new graph)
 * 1 - Enter number of steps
 * 2 - Enter [ramp(minutes from previous temperature), target temperature(Celsius), hold time(minutes)] for each step
 * 3 - Graph. Confirm steps and start kiln or remake steps. After confirmation, save the steps to EEPROM after another confirmation.
 * 3 - Graph (continued). After saving, target Graph is displayed in blue and real time Graph in red.
 * 4 - From 0. Load previously saved graph from EEPROM.
 * 5 - After loading, display the graph. Next is 3.
 */
volatile uint8_t next_task = 0;
volatile uint8_t prev_task = 15; // Initialize to an invalid task number

volatile uint8_t number_of_steps = 0;    // Number of steps in the graph
volatile uint8_t current_step = 0;       // Current step in the graph creation process
volatile uint8_t prev_step = 0b11111111; // Which step was drawn last. To prevent redrawing current_step multiple times.
volatile uint8_t current_digit = 0;      // Keep track of which digit we are working on in any printing function
volatile uint8_t current_entry = 0;      // Keep track of which of the digits, targetTime, holdTime etc is being filled.

// Add these variables at the top of LCD_tasks.cpp with other globals
uint16_t temp_reachTime = 0;  // Temporary storage for reach time entry
uint16_t temp_targetTemp = 0; // Temporary storage for target temp entry
uint16_t temp_holdTime = 0;   // Temporary storage for hold time entry
uint16_t temp_steps = 0;      // Temporary storage for number of steps entry

uint16_t *reachTime = NULL;  // Pointer to dynamically allocated array of reachTimes.
uint16_t *targetTemp = NULL; // Pointer to dynamically allocated array of targetTemps.
uint16_t *holdTime = NULL;   // Pointer to dynamically allocated array of holdTimes.

uint16_t stepscreenx = 0; // x position of data entry for screen 1.
uint16_t stepscreeny = 0; // y position of data entry for screen 1.

uint16_t graphentryX = 0; // x position of entries for new graph.
uint16_t graphentryY = 0; // y position of entries for new graph. Subsqnt. values [reachTime, targetTemp, holdTime] are on newline.

uint16_t graphPointEntryx = 0;
uint16_t graphPointEntryy = 0;

TaskHandle_t LCD_task_handle = NULL; // Task handle for all lcd windows. Use a state machine to switch between windows.

/*
 * Variable to store the current ir command awaiting processing by current task.
 * Initialized to 'X' to indicate no command has been received yet.
 */
static char current_command = 'X';

// With validation
int charToNumber(char ch)
{
    if (ch >= '0' && ch <= '9')
    {
        return ch - '0';
    }
    return 12; // Invalid character
}

void LCD_setup()
{
    // Initialize the LCD display
    // This function should set up the display, initialize any necessary libraries,
    // and prepare the display for use.

    tft.init();
    tft.setRotation(1);

    tft.fillScreen(TFT_BLACK);

    tft.setTextColor(TFT_WHITE, TFT_BLACK); // Adding a background colour erases previous text automatically

    tft.fillScreen(TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(20, 20);
    tft.println("Kiln System");
    tft.println("Initializing...");

    xTaskCreatePinnedToCore(
        LCD_task,
        "Starting screen",
        2048,
        NULL,
        1,
        &LCD_task_handle,
        ARDUINO_RUNNING_CORE);
}

void LCD_task(void *pvParameters)
{
    for (;;)
    {
        switch (next_task)
        {
        case 0: // Start screen
            if (prev_task != 0)
            {
                drawStartScreen();
                prev_task = 0;
            }
            handleStartScreenInput();
            break;

        case 1: // Enter steps
            if (prev_task != 1)
            {
                drawEnterStepsScreen();
                prev_task = 1;
            }
            handleEnterStepsInput();
            break;

        case 2: // Enter steps
            if (prev_task != 2)
            {
                drawEnterPointsForGraph();
                prev_task = 2;
            }
            handleEnterPointsForGraphInput();
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// ... draw start screen ...
void drawStartScreen()
{
    emptyQueue<char>(button_queue);
    // Clear the screen and display the start screen
    tft.fillScreen(TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(20, 20);
    tft.println("Kiln Maestro by");
    tft.println("by Smrutiranjan Sahoo");
    tft.println("and Souryashree Samarpan");
    tft.println(" ");
    tft.println("Press a button to start...");
    tft.println(" ");

    // Provide options for next window
    // tft.setCursor(tft.textWidth("Kiln System by Smrutiranjan Sahoo") + 20, 20);
    tft.println("Load Graph - (1)");
    tft.println(" ");
    tft.println("New Graph - (2)");
}

//...handle number inputs...
void handleStartScreenInput()
{
    if (xQueueReceive(button_queue, &current_command, 10 * portTICK_PERIOD_MS) == pdTRUE)
    {
        switch (current_command)
        {
        case '1':          // Load Graph
            next_task = 4; // Set next task to load graph
            tft.drawRect(0, (240 - 40), tft.width(), tft.fontHeight(), TFT_BLACK);
            tft.setCursor((320 - tft.textWidth("Loading graph...")) / 2, (240 - 40));
            tft.println("Loading graph...");
            break;

        case '2':          // New Graph
            next_task = 1; // Set next task to enter number of steps
            tft.drawRect(0, (240 - 40), tft.width(), tft.fontHeight(), TFT_BLACK);
            tft.setCursor((320 - tft.textWidth("Creating new graph...")) / 2, (240 - 40));
            tft.println("Creating new graph...");
            break;

        default: // Handle other commands or ignore
            tft.drawRect(0, (240 - 40), tft.width(), tft.fontHeight(), TFT_BLACK);
            tft.setCursor((320 - tft.textWidth("Press valid button please...")) / 2, (240 - 40));
            tft.println("Press valid button please...");
            break;
        }
    }
}

// Update drawEnterStepsScreen to use temp_steps
void drawEnterStepsScreen()
{
    current_digit = 0;
    number_of_steps = 0;
    temp_steps = 0; // Reset temporary value
    emptyQueue<char>(button_queue);
    tft.fillScreen(TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor((320 - tft.textWidth("Enter no of steps -    ")) / 2, 120);
    tft.print("Enter no of steps - ");
    stepscreenx = tft.getCursorX();
    stepscreeny = tft.getCursorY();
    tft.print("_"); // Show cursor
}

// Update handleEnterStepsInput to use temp_steps
void handleEnterStepsInput()
{
    if (xQueueReceive(button_queue, &current_command, 50 * portTICK_PERIOD_MS))
    {
        if (current_command == 'B')
        {
            // Remove last digit
            temp_steps /= 10;
            current_digit = (current_digit > 0) ? current_digit - 1 : 0;

            // Update display
            tft.fillRect(stepscreenx, stepscreeny, 100, tft.fontHeight(), TFT_BLACK);
            tft.setCursor(stepscreenx, stepscreeny);
            if (temp_steps > 0)
            {
                tft.print(temp_steps);
            }
            else
            {
                tft.print("_");
            }
        }
        else if (current_command >= '0' && current_command <= '9' && current_digit < 2)
        {
            // Add digit to the right
            int digit = current_command - '0';
            temp_steps = temp_steps * 10 + digit;
            current_digit++;

            // Update display
            tft.fillRect(stepscreenx, stepscreeny, 100, tft.fontHeight(), TFT_BLACK);
            tft.setCursor(stepscreenx, stepscreeny);
            tft.print(temp_steps);
        }
        else if (current_command == 'D')
        {
            temp_steps = 0; // Clear temporary value
            next_task = 0;
            tft.fillRect(0, (240 - 40), tft.width(), tft.fontHeight(), TFT_BLACK);
            tft.setCursor((320 - tft.textWidth("Going back...")) / 2, (240 - 40));
            tft.println("Going back...");
            vTaskDelay(pdMS_TO_TICKS(500));
        }
        else if (current_command == 'O')
        {
            if (temp_steps > 0 && temp_steps <= 10)
            {
                number_of_steps = temp_steps; // Save the value

                // Allocate memory with NULL checks
                reachTime = (uint16_t *)malloc(number_of_steps * sizeof(uint16_t));
                targetTemp = (uint16_t *)malloc(number_of_steps * sizeof(uint16_t));
                holdTime = (uint16_t *)malloc(number_of_steps * sizeof(uint16_t));

                if (reachTime == NULL || targetTemp == NULL || holdTime == NULL)
                {
                    // Handle allocation failure
                    if (reachTime)
                        free(reachTime);
                    if (targetTemp)
                        free(targetTemp);
                    if (holdTime)
                        free(holdTime);
                    reachTime = NULL;
                    targetTemp = NULL;
                    holdTime = NULL;

                    tft.fillRect(0, (240 - 40), tft.width(), 40, TFT_BLACK);
                    tft.setCursor((320 - tft.textWidth("Memory Error!")) / 2, (240 - 40));
                    tft.print("Memory Error!");
                    vTaskDelay(pdMS_TO_TICKS(2000));
                    next_task = 0;
                }
                else
                {
                    // Initialize arrays to 0
                    memset(reachTime, 0, number_of_steps * sizeof(uint16_t));
                    memset(targetTemp, 0, number_of_steps * sizeof(uint16_t));
                    memset(holdTime, 0, number_of_steps * sizeof(uint16_t));

                    temp_steps = 0; // Clear temporary value
                    next_task = 2;
                    tft.fillRect(0, (240 - 40), tft.width(), 40, TFT_BLACK);
                    tft.setCursor((320 - tft.textWidth("Initializing new graph...")) / 2, (240 - 40));
                    tft.println("Initializing new graph...");
                    vTaskDelay(pdMS_TO_TICKS(500));
                }
            }
            else
            {
                // Invalid number of steps
                tft.fillRect(0, (240 - 40), tft.width(), 40, TFT_BLACK);
                tft.setCursor((320 - tft.textWidth("Enter 1-10 steps!")) / 2, (240 - 40));
                tft.print("Enter 1-10 steps!");
            }
        }
    }
}

// Update drawEnterPointsForGraph to reset temp variables
void drawEnterPointsForGraph()
{
    prev_step = 0;
    current_step = 0;
    current_entry = 0;

    // Reset all temporary values
    temp_reachTime = 0;
    temp_targetTemp = 0;
    temp_holdTime = 0;

    emptyQueue<char>(button_queue);
    tft.fillScreen(TFT_BLACK);
    tft.setTextSize(2);

    // Display step number (1-based for display)
    tft.setCursor((320 - tft.textWidth("Step no -   ")) / 2, 40);
    tft.print("Step no - ");
    graphentryX = tft.getCursorX();
    graphentryY = tft.getCursorY();
    tft.print(current_step + 1);

    // Display all three fields with correct labels
    tft.setCursor((320 - tft.textWidth("Enter reach time -     ")) / 2, 100);
    tft.print("Enter reach time - ");
    graphPointEntryx = tft.getCursorX();
    graphPointEntryy = tft.getCursorY();
    tft.print("_");

    tft.setCursor((320 - tft.textWidth("Enter target temp -    ")) / 2, 120);
    tft.print("Enter target temp - ");

    tft.setCursor((320 - tft.textWidth("Enter hold time   -    ")) / 2, 140);
    tft.print("Enter hold time   - ");

    // Instructions
    tft.setTextSize(1);
    tft.setCursor(10, 200);
    tft.print("UP/DOWN: Navigate | OK: Next | BACK: Previous");
    tft.setTextSize(2);

    current_digit = 0;
    highlightCurrentField();
}

// Helper function to get current temp value reference
uint16_t *getCurrentTempValue()
{
    switch (current_entry)
    {
    case 0:
        return &temp_reachTime;
    case 1:
        return &temp_targetTemp;
    case 2:
        return &temp_holdTime;
    default:
        return &temp_reachTime;
    }
}

void highlightCurrentField()
{
    // Clear all field indicators
    tft.fillRect(5, 100, 5, 16, TFT_BLACK);
    tft.fillRect(5, 120, 5, 16, TFT_BLACK);
    tft.fillRect(5, 140, 5, 16, TFT_BLACK);

    // Draw indicator for current field
    int y = 100 + (current_entry * 20);
    tft.fillRect(5, y, 5, 16, TFT_WHITE);
}

void handleEnterPointsForGraphInput()
{
    // Update step number if changed
    if (prev_step != current_step)
    {
        prev_step = current_step;
        tft.fillRect(graphentryX, graphentryY, 60, tft.fontHeight(), TFT_BLACK);
        tft.setCursor(graphentryX, graphentryY);
        tft.print(current_step + 1);

        // Load values from arrays to temp variables
        temp_reachTime = reachTime[current_step];
        temp_targetTemp = targetTemp[current_step];
        temp_holdTime = holdTime[current_step];

        // Clear all value fields when changing steps
        tft.fillRect(graphPointEntryx, graphPointEntryy, 60, tft.fontHeight(), TFT_BLACK);
        tft.fillRect(graphPointEntryx, graphPointEntryy + 20, 60, tft.fontHeight(), TFT_BLACK);
        tft.fillRect(graphPointEntryx, graphPointEntryy + 40, 60, tft.fontHeight(), TFT_BLACK);

        // Display existing values
        tft.setCursor(graphPointEntryx, graphPointEntryy);
        if (temp_reachTime > 0)
            tft.print(temp_reachTime);
        else
            tft.print("_");

        tft.setCursor(graphPointEntryx, graphPointEntryy + 20);
        if (temp_targetTemp > 0)
            tft.print(temp_targetTemp);
        else
            tft.print("_");

        tft.setCursor(graphPointEntryx, graphPointEntryy + 40);
        if (temp_holdTime > 0)
            tft.print(temp_holdTime);
        else
            tft.print("_");

        // Update current_digit based on current entry value
        uint16_t *currentVal = getCurrentTempValue();
        current_digit = (*currentVal > 0) ? floor(log10(*currentVal)) + 1 : 0;
    }

    if (xQueueReceive(button_queue, &current_command, 50 * portTICK_PERIOD_MS))
    {
        int yOffset = current_entry * 20;
        uint16_t *currentTempValue = getCurrentTempValue();

        if (current_command == 'B') // BACK button
        {
            if (*currentTempValue == 0 && current_entry == 0 && current_step == 0)
            {
                // Go back to previous screen
                next_task = 1;
                // Clear temp values
                temp_reachTime = 0;
                temp_targetTemp = 0;
                temp_holdTime = 0;
                // Free allocated memory
                if (reachTime)
                {
                    free(reachTime);
                    reachTime = NULL;
                }
                if (targetTemp)
                {
                    free(targetTemp);
                    targetTemp = NULL;
                }
                if (holdTime)
                {
                    free(holdTime);
                    holdTime = NULL;
                }
            }
            else if (*currentTempValue == 0)
            {
                // Save current values before moving
                reachTime[current_step] = temp_reachTime;
                targetTemp[current_step] = temp_targetTemp;
                holdTime[current_step] = temp_holdTime;

                // Move to previous field or step
                if (current_entry > 0)
                {
                    current_entry--;
                }
                else if (current_step > 0)
                {
                    current_step--;
                    current_entry = 2;
                }

                currentTempValue = getCurrentTempValue();
                current_digit = (*currentTempValue > 0) ? floor(log10(*currentTempValue)) + 1 : 0;
                highlightCurrentField();
            }
            else
            {
                // Remove last digit
                *currentTempValue /= 10;
                current_digit = (current_digit > 0) ? current_digit - 1 : 0;

                // Update display
                tft.fillRect(graphPointEntryx, graphPointEntryy + yOffset, 60, tft.fontHeight(), TFT_BLACK);
                tft.setCursor(graphPointEntryx, graphPointEntryy + yOffset);
                if (*currentTempValue > 0)
                    tft.print(*currentTempValue);
                else
                    tft.print("_");
            }
        }
        else if (current_command >= '0' && current_command <= '9' && current_digit < 4)
        {
            // Add digit
            int digit = current_command - '0';
            *currentTempValue = *currentTempValue * 10 + digit;
            current_digit++;

            // Update display
            tft.fillRect(graphPointEntryx, graphPointEntryy + yOffset, 60, tft.fontHeight(), TFT_BLACK);
            tft.setCursor(graphPointEntryx, graphPointEntryy + yOffset);
            tft.print(*currentTempValue);
        }
        else if (current_command == 'U') // UP arrow
        {
            if (current_entry > 0)
            {
                // Save current values to arrays
                reachTime[current_step] = temp_reachTime;
                targetTemp[current_step] = temp_targetTemp;
                holdTime[current_step] = temp_holdTime;

                current_entry--;
                currentTempValue = getCurrentTempValue();
                current_digit = (*currentTempValue > 0) ? floor(log10(*currentTempValue)) + 1 : 0;
                highlightCurrentField();

                // Update display
                int newYOffset = current_entry * 20;
                tft.fillRect(graphPointEntryx, graphPointEntryy + newYOffset, 60, tft.fontHeight(), TFT_BLACK);
                tft.setCursor(graphPointEntryx, graphPointEntryy + newYOffset);
                if (*currentTempValue > 0)
                    tft.print(*currentTempValue);
                else
                    tft.print("_");
            }
        }
        else if (current_command == 'D') // DOWN arrow
        {
            if (current_entry < 2)
            {
                // Save current values to arrays
                reachTime[current_step] = temp_reachTime;
                targetTemp[current_step] = temp_targetTemp;
                holdTime[current_step] = temp_holdTime;

                current_entry++;
                currentTempValue = getCurrentTempValue();
                current_digit = (*currentTempValue > 0) ? floor(log10(*currentTempValue)) + 1 : 0;
                highlightCurrentField();

                // Update display
                int newYOffset = current_entry * 20;
                tft.fillRect(graphPointEntryx, graphPointEntryy + newYOffset, 60, tft.fontHeight(), TFT_BLACK);
                tft.setCursor(graphPointEntryx, graphPointEntryy + newYOffset);
                if (*currentTempValue > 0)
                    tft.print(*currentTempValue);
                else
                    tft.print("_");
            }
        }
        else if (current_command == 'O') // OK button
        {
            // Save all current values
            reachTime[current_step] = temp_reachTime;
            targetTemp[current_step] = temp_targetTemp;
            holdTime[current_step] = temp_holdTime;

            // Move to next field or step
            if (current_entry < 2)
            {
                current_entry++;
                currentTempValue = getCurrentTempValue();
                current_digit = (*currentTempValue > 0) ? floor(log10(*currentTempValue)) + 1 : 0;
                highlightCurrentField();

                // Update display
                int newYOffset = current_entry * 20;
                tft.fillRect(graphPointEntryx, graphPointEntryy + newYOffset, 60, tft.fontHeight(), TFT_BLACK);
                tft.setCursor(graphPointEntryx, graphPointEntryy + newYOffset);
                if (*currentTempValue > 0)
                    tft.print(*currentTempValue);
                else
                    tft.print("_");
            }
            else if (current_step < number_of_steps - 1) // -1 because 0-based
            {
                current_step++;
                current_entry = 0;
                current_digit = 0;
                // Reset temp values for new step
                temp_reachTime = 0;
                temp_targetTemp = 0;
                temp_holdTime = 0;
                // prev_step will be updated on next iteration
            }
            else
            {
                // All steps completed
                next_task = 3;
                tft.fillRect(0, 220, 320, 20, TFT_BLACK);
                tft.setCursor((320 - tft.textWidth("All steps entered!")) / 2, 220);
                tft.print("All steps entered!");
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
        }
    }
}
