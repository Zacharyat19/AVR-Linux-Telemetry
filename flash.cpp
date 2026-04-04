#include <stdint.h>

/* * HARDWARE PIN MAPPING
 * The ATmega32U4 Port B physical layout does not match standard Arduino numbering.
 * This array maps the Arduino digital pins to the physical LED sequence 
 * on Bens Board 1.0, ensuring a true "Left to Right" physical orientation.
 * Physical sequence: [D5, D6, D7, D8] followed by [D1, D2, D3, D4]
 */
const uint8_t LED_PINS[] = {11, 10, 9, 8, 14, 16, 15, 17};

/* * STATUS INDICATOR
 * Pin 11 corresponds to the far-left LED (D5). 
 * This is dedicated to displaying the health of the photo_server process.
 */
const uint8_t STATUS_LED = 11; 

void setup() 
{
    // Initialize Native USB (CDC-ACM) at 115200 baud.
    // J1 must be used to communicate with the Raspberry Pi.
    Serial.begin(115200);

    // Initialize all 8 LED pins as digital outputs and force them LOW (OFF)
    // to clear any residual states from the bootloader or previous resets.
    for (uint8_t i = 0; i < 8; i++) 
    {
        pinMode(LED_PINS[i], OUTPUT);
        digitalWrite(LED_PINS[i], LOW);
    }

    /* * POWER-ON SELF TEST (POST)
     * Performs a visual sweep from left to right. 
     * 1. Confirms the 8MHz crystal timing is correct (LilyPad profile).
     * 2. Verifies all GPIO pins and LEDs are electrically functional.
     */
    for (uint8_t i = 0; i < 8; i++) 
    {
        digitalWrite(LED_PINS[i], HIGH);
        delay(70); // 70ms creates a smooth, readable sweep speed
        digitalWrite(LED_PINS[i], LOW);
    }
}

void loop() 
{
    // Check the hardware serial buffer for incoming bytes from the Linux daemon.
    if (Serial.available() > 0) 
    {
        // Read exactly one byte from the buffer.
        uint8_t cmd = Serial.read();

        // 0x01: Command received indicating the process is RUNNING.
        if (cmd == 0x01) 
        {
            digitalWrite(STATUS_LED, HIGH);
        } 
        // 0x00: Command received indicating the process is DOWN or missing.
        else if (cmd == 0x00) 
        {
            digitalWrite(STATUS_LED, LOW);
        }
    }
}