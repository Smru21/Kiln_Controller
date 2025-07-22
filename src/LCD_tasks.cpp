// File: src/LCD_tasks.cpp

#include "LCD_tasks.h"

TFT_eSPI tft = TFT_eSPI();

/*
    * 0 - Startscreen(Load or create new graph)
    * 1 - Enter number of steps
    * 2 - Enter [ramp(minutes from previous temperature), target temperature(Celsius), hold time(minutes)] for each step
    * 3 - Graph. Confirm steps and start kiln or remake steps. After confirmation, save the steps to EEPROM after another confirmation.
    * 3 - Graph (continued). After saving, target Graph is displayed in blue and real time Graph in red.
    * 4 - From 2. Load previously saved graph from EEPROM.
    * 5 - After loading, display the graph. Next is 3.
*/
uint8_t next_task = 0;
uint8_t prev_task = 15; // Initialize to an invalid task number

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
    tft.setCursor(10, 10);
    tft.println("Kiln System");
    tft.println("Initializing...");
}

void LCD_startscreen(void *pvParameters)
{
    for (;;)
    {
        if(next_task == 0 && prev_task != 0)
        {
            // Clear the screen and display the start screen
            tft.fillScreen(TFT_BLACK);
            tft.setTextSize(2);
            tft.setCursor(10, 10);
            tft.println("Kiln System");
            tft.println("Welcome to the Kiln System!");
            tft.println("Press a button to start...");
        }
    }
}
