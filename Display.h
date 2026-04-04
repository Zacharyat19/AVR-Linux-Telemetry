#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>
#include <stdbool.h>

/*
 * HARDWARE PAYLOAD DEFINITIONS
 * Defines the strict 1-byte communication protocol between the 
 * Linux telemetry daemon and the ATmega32U4 microcontroller.
 */
#define PAYLOAD_SIZE 1 

/* * 0x00 (00000000): Command byte instructing the AVR to extinguish the status LED. 
 */
#define PROCESS_DOWN 0x00 

/* * 0x01 (00000001): Command byte instructing the AVR to illuminate the status LED. 
 */
#define PROCESS_UP   0x01 

/*
 * ==========================================
 * TELEMETRY API
 * ==========================================
 */

/*
 * process_is_running
 * Scans the Linux /proc virtual filesystem to verify if a target application is alive.
 * Returns true if the process exists, false if missing or inaccessible.
 */
bool process_is_running(const char* process_name);

/*
 * serial_init
 * Configures the POSIX termios hardware interface for USB CDC-ACM communication.
 * Sets the baud rate to 115200, 8N1, and disables canonical mode.
 * Returns a valid file descriptor on success, or -1 on failure.
 */
int serial_init(const char* device_path);

/*
 * serial_transmit
 * Pushes the designated 1-byte payload state across the hardware bridge.
 */
void serial_transmit(int file_descriptor, uint8_t payload);

#endif