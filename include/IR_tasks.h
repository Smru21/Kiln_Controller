#ifndef IR_TASKS_H
#define IR_TASKS_H

/*
 *  SimpleReceiver.cpp
 *
 *  Receives IR protocol data of 15 main protocols.
 *
 *  *****************************************************************************************************************************
 *  To access the library files from your sketch, you have to first use `Sketch > Show Sketch Folder (Ctrl+K)` in the Arduino IDE.
 *  Then navigate to the parallel `libraries` folder and select the library you want to access.
 *  The library files itself are located in the `src` sub-directory.
 *  If you did not yet store the example as your own sketch, then with Ctrl+K you are instantly in the right library folder.
 *  *****************************************************************************************************************************
 *
 *
 *  The following IR protocols are enabled by default:
 *      Sony SIRCS
 *      NEC + APPLE
 *      Samsung + Samsg32
 *      Kaseikyo
 *
 *      Plus 11 other main protocols by including irmpMain15.h instead of irmp.h
 *      JVC, NEC16, NEC42, Matsushita, DENON, Sharp, RC5, RC6 & RC6A, IR60 (SDA2008) Grundig, Siemens Gigaset, Nokia
 *
 *  To disable one of them or to enable other protocols, specify this before the "#include <irmp.h>" line.
 *  If you get warnings of redefining symbols, just ignore them or undefine them first (see Interrupt example).
 *  The exact names can be found in the library file irmpSelectAllProtocols.h (see Callback example).
 *
 *  Copyright (C) 2019  Armin Joachimsmeyer
 *  armin.joachimsmeyer@gmail.com
 *
 *  This file is part of IRMP https://github.com/IRMP-org/IRMP.
 *
 *  IRMP is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/gpl.html>.
 *
 */

#include <Arduino.h>
#include "Serial_debugging.h" // Include serial debugging functions

/*
 * Set input pin and output pin definitions etc.
 */
#include "Pin_definitions.h"

#define IRMP_PROTOCOL_NAMES 1 // Enable protocol number mapping to protocol strings - requires some FLASH. Must before #include <irmp*>

// #include <irmpSelectMain15Protocols.h>  // This enables 15 main protocols
#define IRMP_SUPPORT_NEC_PROTOCOL 1 // this enables only one protocol
// #define IRMP_SUPPORT_SIRCS_PROTOCOL      1 // this enables only one protocol

/*
 * We use LED_BUILTIN as feedback for commands 0x40 and 0x48 and cannot use it as feedback LED for receiving
 */
#if defined(ALTERNATIVE_IR_FEEDBACK_LED_PIN)
#define IRMP_FEEDBACK_LED_PIN ALTERNATIVE_IR_FEEDBACK_LED_PIN
#endif

void IRMP_setup();
void IR_loop();

#endif // IR_TASKS_H