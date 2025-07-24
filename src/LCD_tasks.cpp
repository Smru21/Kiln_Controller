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

volatile uint8_t number_of_steps = 0; // Number of steps in the graph
volatile uint8_t current_step = 0;    // Current step in the graph creation process
volatile uint8_t current_digit = 0;   // Keep track of which digit we are working on in any printing function

uint16_t stepscreenx = 0; // x position of data entry for screen 1.
uint16_t stepscreeny = 0; // y position of data entry for screen 1.

uint16_t graphentryX = 0; // x position of entries for new graph.
uint16_t graphentryY = 0; // y position of entries for new graph. Subsqnt. values [reachTime, targetTemp, holdTime] are on newline.

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

            // Add more cases for other screens
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

// ... draw enter steps screen ...
void drawEnterStepsScreen()
{
    current_digit = 0;
    number_of_steps = 0;
    emptyQueue<char>(button_queue);
    tft.fillScreen(TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor((320 - tft.textWidth("Enter no of steps -    ")) / 2, 120); // Center the text
    tft.print("Enter no of steps - ");
    stepscreenx = tft.getCursorX();
    stepscreeny = tft.getCursorY();
}

//...handle number inputs in steps screen...
void handleEnterStepsInput()
{
    if (xQueueReceive(button_queue, &current_command, 50 * portTICK_PERIOD_MS))
    {
        if (current_command == 'B')
        {
            // Remove last digit
            number_of_steps /= 10;
            current_digit = (current_digit > 0) ? current_digit - 1 : 0;

            // Update display
            tft.fillRect(stepscreenx, stepscreeny, 100, tft.fontHeight(), TFT_BLACK);
            tft.setCursor(stepscreenx, stepscreeny);
            if (number_of_steps > 0)
            {
                tft.print(number_of_steps);
            }
            else
            {
                tft.print("_");
            }
        }

        else if (current_command >= '0' && current_command <= '9')
        {
            // Add digit to the right
            int digit = current_command - '0';
            number_of_steps = number_of_steps * 10 + digit;
            current_digit++;

            // Update display
            tft.fillRect(stepscreenx, stepscreeny, 100, tft.fontHeight(), TFT_BLACK);
            tft.setCursor(stepscreenx, stepscreeny);
            tft.print(number_of_steps);
        }

        else if (current_command == '*')
        {
            next_task = 0;
            // tft.setCursor((320 - tft.textWidth("Going back...")) / 2, (240 - 40));
            tft.fillRect(0, (240 - 40), tft.width(), tft.fontHeight(), TFT_BLACK);
            tft.setCursor((320 - tft.textWidth("Going back...")) / 2, (240 - 40));
            tft.println("Going back...");
        }
    }
}
