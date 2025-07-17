#include "main.h"
#include "Serial_debugging.h"

void setup()
{
  // Initialize serial debugging
  serial_debugging_init();

  // Set up IRMP library
  IR_setup();

  // Print a message indicating setup is complete
  serial_debugging_print("IRMP setup complete. Ready to receive IR signals.");

}

void loop()
{
  // The IR loop task will handle IR signal processing
  // No need for additional code here as the task runs independently
  vTaskDelay(pdMS_TO_TICKS(1000)); // Delay to prevent watchdog timeout, adjust as necessary
}
