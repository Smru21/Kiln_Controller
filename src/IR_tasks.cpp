#include "IR_tasks.h"
#include <irmp.hpp> // Include IRMP library

// Only define irmp_data here, NOT in the header!
IRMP_DATA irmp_data;

void IRMP_setup()
{
    // Initialize IRMP library
    irmp_init();

    // Initialize serial debugging
    serial_debugging_init(); // Initialize serial debugging

    // Set up the input pin for receiving IR signals
    pinMode(IRMP_INPUT_PIN, INPUT);

    // Initialize serial debugging
    Serial.print(F("Ready to receive IR signals of protocols: "));
    irmp_print_active_protocols(&Serial);
    Serial.println(F("at pin " STR(IRMP_INPUT_PIN)));
}

void IR_loop()
{
    /*
     * Check if new data available and get them
     */
    if (irmp_get_data(&irmp_data))
    {
        /*
         * Skip repetitions of command
         */
        // if (!(irmp_data.flags & IRMP_FLAG_REPETITION))
        // {
        //     /*
        //      * Here data is available and is no repetition -> evaluate IR command
        //      */

        // }
        irmp_result_print(&irmp_data);
    }
}
