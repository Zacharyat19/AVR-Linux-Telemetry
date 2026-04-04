#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define PAYLOAD_SIZE 6
#define DISPLAY_NUM_0 0xC0
#define DISPLAY_NUM_1 0xF9

bool process_is_running(const char* process_name);
int serial_init(const char* device_path);
void serial_transmit(int file_descriptor, uint8_t payload);

#endif