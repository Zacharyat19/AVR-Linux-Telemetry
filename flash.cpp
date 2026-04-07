/**
 * ============================================================================
 * Hardware-Software Bridge: AVR Telemetry Firmware
 * ============================================================================
 * Target: ATmega32U4 (Custom "Bens Board" PCB)
 * Description: Listens to a POSIX serial interface to display system telemetry 
 * on a hardware SPI ST7565 LCD and toggles indicator LEDs. 
 * Monitors a physical button to send interrupt signals upstream.
 * ============================================================================
 */

#include <stdint.h>
#include <SPI.h>
#include <U8x8lib.h> 

/**
 * HARDWARE CONFIGURATION
 * Pin mappings correspond to the custom ATmega32U4 PCB traces.
 */
const uint8_t LED_PINS[] = {11, 10, 9, 8}; // D5, D6, D7, D8 on the schematic
const uint8_t BUTTON_PIN = 6;              // S4 on the schematic
const uint8_t LCD_BACKLIGHT = 13;          // PC7 on the schematic

/**
 * LCD CONFIGURATION
 * Software SPI constructor is used to bypass the default Arduino core pin mappings.
 * The NHD profile is specifically required to ensure clearDisplay() wipes memory
 * starting at Column 0, preventing edge corruption on this specific screen.
 * Pins: clock=15, data=16, cs=17, dc=A4, reset=A5
 */
U8X8_ST7565_NHD_C12864_4W_SW_SPI u8x8(15, 16, 17, A4, A5);

/**
 * UPSTREAM STATE: Button Debouncing
 */
int button_state = HIGH;         
int last_reading = HIGH;         
unsigned long last_debounce_time = 0;
const unsigned long DEBOUNCE_DELAY = 50; 

/**
 * DOWNSTREAM STATE: Serial Protocol Buffers
 */
bool reading_lcd_string = false;
String lcd_buffer = "";
uint8_t target_row = 0; // Tracks whether to write to the top (0) or bottom (2) row

void setup() 
{
    // High baud rate for low-latency host communication
    Serial.begin(115200);

    // Initialize telemetry LEDs
    for (uint8_t i = 0; i < 4; i++) 
    {
        pinMode(LED_PINS[i], OUTPUT);
        digitalWrite(LED_PINS[i], LOW); 
    }
    
    // Physical button uses internal pull-up resistor
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    
    // Force backlight off to rely on ambient lighting
    pinMode(LCD_BACKLIGHT, OUTPUT);
    digitalWrite(LCD_BACKLIGHT, LOW); 

    // Initialize the ST7565R LCD
    u8x8.begin();
    
    /**
     * MANUAL HARDWARE OVERRIDES
     * Replicates the exact ST7565 initialization bytes from the custom AVR 
     * assembly driver to prevent voltage saturation (black bars).
     */
    u8x8.sendF("c", 0x22); // Resistor ratio (Voltage bias)
    u8x8.sendF("c", 0x81); // Contrast command prefix
    u8x8.sendF("c", 15);   // Contrast volume value
    
    // Rotate 180 degrees to match the physical PCB mounting
    u8x8.setFlipMode(1); 

    // Wipe uninitialized static from the screen RAM
    u8x8.clearDisplay();

    u8x8.setFont(u8x8_font_chroma48medium8_r); 
    
    // Initial display state (Shifted x to 1 to avoid bezel dead zones)
    u8x8.drawString(1, 0, "Pi Telemetry");
    u8x8.drawString(1, 2, "Waiting..."); 
}

void loop() 
{
    /**
     * 1. DOWNSTREAM: Serial Router
     * Parses incoming bytes from the host daemon. Uses a while loop to 
     * rapidly empty the buffer when full strings arrive.
     */
    while (Serial.available() > 0) 
    {
        uint8_t cmd = Serial.read();

        // ROUTE A: Currently capturing characters for the LCD display
        if (reading_lcd_string) 
        {
            if (cmd == '\n') 
            {
                // Clear the target row by printing spaces starting at 0
                u8x8.drawString(0, target_row, "                "); 
                
                // Print the new string starting at 1 to avoid the bezel
                u8x8.drawString(1, target_row, lcd_buffer.c_str());
                
                // Reset flags for the next incoming command
                lcd_buffer = "";
                reading_lcd_string = false;
            } 
            else 
            {
                // Append the incoming ASCII character to the buffer
                lcd_buffer += (char)cmd;
            }
        } 
        // ROUTE B: Listening for standard control bytes
        else 
        {
            if (cmd == 0x21) 
            { 
                // 0x21 -> Switch to LCD string capture mode (Top Line)
                reading_lcd_string = true;
                target_row = 0;
                lcd_buffer = "";
            }
            else if (cmd == 0x22) 
            { 
                // 0x22 -> Switch to LCD string capture mode (Bottom Line)
                reading_lcd_string = true;
                target_row = 2; // Visually the second text line in U8x8
                lcd_buffer = "";
            }
            else if (cmd >= 0x10 && cmd <= 0x13) 
            {
                // 0x10 to 0x13 -> Turn LED Slot [0-3] ON
                digitalWrite(LED_PINS[cmd - 0x10], HIGH);
            }
            else if (cmd >= 0x00 && cmd <= 0x03) 
            {
                // 0x00 to 0x03 -> Turn LED Slot [0-3] OFF
                digitalWrite(LED_PINS[cmd - 0x00], LOW);
            }
        }
    }

    /**
     * 2. UPSTREAM: Restart Button Logic
     * Standard non-blocking debounce algorithm to filter mechanical noise.
     */
    int current_reading = digitalRead(BUTTON_PIN);

    // If the physical pin changed, reset the timer
    if (current_reading != last_reading) last_debounce_time = millis();

    // Only process the reading if it has been stable for longer than DEBOUNCE_DELAY
    if ((millis() - last_debounce_time) > DEBOUNCE_DELAY) 
    {
        // If the stabilized reading is different from our current state
        if (current_reading != button_state) 
        {
            button_state = current_reading;
            
            // If the final stabilized state moved to LOW, the button was pressed
            if (button_state == LOW) 
            { 
                Serial.write('R'); // Send 'Restart' interrupt signal to host
            }
        }
    }
    
    // Save the reading for the next iteration
    last_reading = current_reading;
}