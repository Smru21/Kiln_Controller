#ifndef REMOTE_BUTTONS_H
#define REMOTE_BUTTONS_H

#include <stdint.h>
#include <Arduino.h>

#define ROBU_BLACK_BLUE_RED_17 // Define ROBU Black Blue Red 17 remote button mapping

// Define button characters
#define BUTTON_1 '1'
#define BUTTON_2 '2'
#define BUTTON_3 '3'
#define BUTTON_4 '4'
#define BUTTON_5 '5'
#define BUTTON_6 '6'
#define BUTTON_7 '7'
#define BUTTON_8 '8'
#define BUTTON_9 '9'
#define BUTTON_0 '0'

#define BUTTON_ASTERISK '*'
#define BUTTON_HASH '#'

#define BUTTON_UP 'U'
#define BUTTON_DOWN 'D'
#define BUTTON_OK 'O'
#define BUTTON_BACK 'B'
#define BUTTON_NEXT 'N'

#define BUTTON_UNKNOWN 'X'

char commandToButton(uint16_t command);
#endif
