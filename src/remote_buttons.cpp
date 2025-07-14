#include "remote_buttons.h"

#ifdef ROBU_BLACK_BLUE_RED_17
/**
 * Converts a command code to a button character for the ROBU Black Blue Red 17 remote.
 *
 * @param command The command code to convert.
 * @return The corresponding button character or BUTTON_UNKNOWN if the command is not recognized.
 */
char commandToButton(uint16_t command)
{
    switch (command)
    {
    case 0x45:
        return BUTTON_1;
    case 0x46:
        return BUTTON_2;
    case 0x47:
        return BUTTON_3;
    case 0x44:
        return BUTTON_4;
    case 0x40:
        return BUTTON_5;
    case 0x43:
        return BUTTON_6;
    case 0x07:
        return BUTTON_7;
    case 0x15:
        return BUTTON_8;
    case 0x09:
        return BUTTON_9;
    case 0x19:
        return BUTTON_0;
    case 0x18:
        return BUTTON_UP;
    case 0x52:
        return BUTTON_DOWN;
    case 0x1C:
        return BUTTON_OK;
    case 0x08:
        return BUTTON_BACK;
    case 0x5A:
        return BUTTON_NEXT;
    case 0x16:
        return BUTTON_ASTERISK;
    case 0x0D:
        return BUTTON_HASH;
    default:
        return BUTTON_UNKNOWN; // Return unknown for unrecognized commands
    }
}

#endif
