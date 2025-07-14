#define IRMP_INPUT_PIN 27      // D27
#define IRSND_OUTPUT_PIN 4     // D4
#define ROBU_BLACK_BLUE_RED_17 // Define this to use the commandToButton function for ROBU Black Blue Red 17 remote
#define TONE_LEDC_CHANNEL 1    // Using channel 1 makes tone() independent of receiving timer -> No need to stop receiving timer.
// tone() is included in ESP32 core since 2.0.2

/*
 * Helper macro for getting a macro definition as string
 */
#if !defined(STR_HELPER)
#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
#endif

#include <Arduino.h>
#include <remote_buttons.h> // Include the remote button definitions
/*
 * Set input pin and output pin definitions etc.
 */

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
/*
 * After setting the definitions we can include the code and compile it.
 */
#include <irmp.hpp>

IRMP_DATA irmp_data;

QueueHandle_t irmp_queue;                   // Queue for IRMP data
TaskHandle_t print_ir_commands_task_handle; // Task handle for printing IR commands

void print_ir_commands(void *pvParameters)
{
  static uint16_t ircode;
  while (true)
  {
    if (xQueueReceive(irmp_queue, &ircode, 10 * portTICK_PERIOD_MS) == pdTRUE)
    {
      Serial.print(F("Pressed Button: "));
      Serial.println(commandToButton(ircode)); // Convert command to button character and print it
    }
    vTaskDelay(100 / portTICK_PERIOD_MS); // Delay to avoid busy waiting
  }
}

void setup()
{
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);

  // Just to know which program is running on my Arduino
  Serial.println(F("START " __FILE__ " from " __DATE__ "\r\nUsing library version " VERSION_IRMP));

  irmp_init();

  irmp_queue = xQueueCreate(10, sizeof(uint16_t)); // Create a queue to hold IR codes

  xTaskCreatePinnedToCore(print_ir_commands,
                          "PrintIRCommands",
                          2048,
                          NULL,
                          1,
                          &print_ir_commands_task_handle,
                          1); // Create a task to print IR commands

  Serial.print(F("Ready to receive IR signals of protocols: "));
  irmp_print_active_protocols(&Serial);
  Serial.println(F("at pin " STR(IRMP_INPUT_PIN)));
}

void loop()
{
  /*
   * Check if new data available and get them
   */
  if (irmp_get_data(&irmp_data))
  {
    /*
     * Skip repetitions of command
     */
    if (irmp_data.flags & IRMP_FLAG_REPETITION)
    {
      uint16_t command = irmp_data.command;
      xQueueSend(irmp_queue, &command, 10 * portTICK_PERIOD_MS); // Send data to queue and wait up to 10 ms
    }
  }
}
