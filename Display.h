#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>
#include <stddef.h>

/**
 * PROTOCOL DEFINITIONS
 * These offsets are added to the Slot ID (0-3) to create the hex command
 * sent over the serial bus.
 * Example: ON command for Slot 0 is 0x10 + 0 = 0x10
 * Example: OFF command for Slot 2 is 0x00 + 2 = 0x02
 */
#define CMD_ON  0x10
#define CMD_OFF 0x00

/**
 * HARDWARE MAPPING
 * Defines which physical LED slot on the controller corresponds to 
 * the photo-server process.
 */
#define PHOTO_SERVER_SLOT 0

/**
 * Initializes the serial port for communication.
 * * @param port The device path (e.g., "/dev/ttyACM0").
 * @return The file descriptor (fd) on success, or -1 on failure.
 */
int serial_init(const char* port);

/**
 * Formats and sends a single-byte command to the Arduino.
 * * @param fd    The active serial file descriptor.
 * @param slot  The target LED slot (0 through 3).
 * @param state 1 for ON, 0 for OFF.
 */
void serial_transmit(int fd, uint8_t slot, uint8_t state);

/**
 * Scans the Linux /proc filesystem to check if a process is active.
 * * @param process_name The string to search for in the process command line.
 * @return 1 if found (running), 0 if not found (down).
 */
int process_is_running(const char* process_name);

/**
 * Calculates the available storage space on the root filesystem and current CPU temp
 * and formats them into strings for the LCD display.
 * * @param total_buf Buffer to store the formatted total space string (Line 1).
 * @param free_buf  Buffer to store the formatted free space string (Line 2).
 * @param max_len   The maximum size of the provided buffers.
 */
void get_system_metrics(char* line1_buf, char* line2_buf, size_t max_len);

#endif