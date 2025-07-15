#include "main.h"
#include "Serial_debugging.h"

void setup()
{
  // Initialize serial debugging
  serial_debugging_init();

  // Set up IRMP library
  IRMP_setup();

  // Print a message indicating setup is complete
  serial_debugging_print("IRMP setup complete. Ready to receive IR signals.");

}

void loop()
{
  // Continuously check for IR signals
  IR_loop();
}
