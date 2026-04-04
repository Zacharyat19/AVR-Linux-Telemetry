#include <stdio.h>
#include <unistd.h>

#include "Display.h"

int main()
{
    int fd = serial_init("/dev/ttyACM0");
    if (fd < 0) return 1;

    while (1)
    {
        // Evaluate the target process
        uint8_t current_status = PROCESS_DOWN;

        if (process_is_running("photo_server")) current_status = PROCESS_UP;

        // Update the hardware
        serial_transmit(fd, current_status);

        // Rest for 1 second before checking again
        sleep(1);
    }

    close(fd);
    return 0;
}