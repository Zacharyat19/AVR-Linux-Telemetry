#include <stdint.h>

/**
 * HARDWARE CONFIGURATION
 * LED_PINS map to the first four telemetry slots on the board.
 * BUTTON_PIN maps to S4 (Physical Pin 6 / PD7) based on the schematic.
 */
const uint8_t LED_PINS[] = {11, 10, 9, 8};
const uint8_t BUTTON_PIN = 6; 

/**
 * DEBOUNCE & STATE VARIABLES
 * Used to filter out mechanical noise from the tactile switch.
 */
int button_state = HIGH;         // Current filtered state (starts HIGH due to pull-up)
int last_reading = HIGH;         // Previous raw reading from the pin
unsigned long last_debounce_time = 0;
const unsigned long DEBOUNCE_DELAY = 50; // Milliseconds to wait for signal to stabilize

void setup() 
{
    // Higher baud rate for low latency communication with the Pi
    Serial.begin(115200);

    // Initialize telemetry LEDs
    for (uint8_t i = 0; i < 4; i++) 
    {
        pinMode(LED_PINS[i], OUTPUT);
        digitalWrite(LED_PINS[i], LOW); // Start with LEDs OFF (no flickering)
    }
    
    // Internal pull-up resistor keeps the pin HIGH until the button pulls it to Ground (LOW)
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    
    // NOTE: POST (Power-On Self-Test) sweep removed to keep the display static on startup.
}

void loop() 
{
    /**
     * 1. DOWNSTREAM: Slot-based Telemetry
     * Listens for single-byte commands from the Pi daemon.
     * Protocol:
     * 0x10 - 0x13 -> Turn Slot [0-3] ON
     * 0x00 - 0x03 -> Turn Slot [0-3] OFF
     */
    if (Serial.available() > 0) 
    {
        uint8_t cmd = Serial.read();

        // Check if command is in the 'ON' range
        if (cmd >= 0x10 && cmd <= 0x13) {
            digitalWrite(LED_PINS[cmd - 0x10], HIGH);
        }
        // Check if command is in the 'OFF' range
        else if (cmd >= 0x00 && cmd <= 0x03) {
            digitalWrite(LED_PINS[cmd - 0x00], LOW);
        }
    }

    /**
     * 2. UPSTREAM: Restart Button Logic
     * Standard non-blocking debounce algorithm.
     */
    int current_reading = digitalRead(BUTTON_PIN);

    // If the physical pin changed, reset the timer
    if (current_reading != last_reading) {
        last_debounce_time = millis();
    }

    // Only process the reading if it has been stable for longer than DEBOUNCE_DELAY
    if ((millis() - last_debounce_time) > DEBOUNCE_DELAY) 
    {
        // If the stabilized reading is different from our current state
        if (current_reading != button_state) 
        {
            button_state = current_reading;
            
            // If the final stabilized state moved to LOW, the button was pressed
            if (button_state == LOW) { 
                Serial.write('R'); // Send 'Restart' signal to the Pi daemon
            }
        }
    }
    
    // Save the reading for the next iteration
    last_reading = current_reading;
}