#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define PAYLOAD_SIZE 1 
#define PROCESS_DOWN 0x00 // Binary 00000000 (All LEDs OFF)
#define PROCESS_UP   0x01 // Binary 00000001 (Turns ON LED D1)

bool process_is_running(const char* process_name);
int serial_init(const char* device_path);
void serial_transmit(int file_descriptor, uint8_t payload);

#endif