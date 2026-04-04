#include <stdio.h>
#include <unistd.h>

#include "Display.h"

int main()
{
    /*
     * INITIALIZATION PHASE
     * Open the native USB port (CDC-ACM) connected to the ATmega32U4.
     * The serial_init function handles the POSIX termios configuration,
     * setting the line to 115200 baud, 8N1, in raw output mode.
     */
    int fd = serial_init("/dev/ttyACM0");
    if (fd < 0) return 1;

    /*
     * TELEMETRY LOOP
     * This infinite loop runs continuously in the background as a daemon process.
     */
    while (1)
    {
        /* * FAIL-SAFE DEFAULT
         * Always assume the process is down until proven otherwise. 
         * This prevents false positives if the process check fails unexpectedly.
         */
        uint8_t current_status = PROCESS_DOWN;

        /*
         * SYSTEM EVALUATION
         * Scan the Linux /proc virtual filesystem to determine if the 
         * target process is currently active in the kernel's process table.
         */
        if (process_is_running("photo_server")) current_status = PROCESS_UP;

        /*
         * HARDWARE UPDATE
         * Push the 1-byte payload across the USB bus to the AVR microcontroller.
         */
        serial_transmit(fd, current_status);

        /*
         * CPU YIELD
         * Suspend thread execution for 1 second.
         * This prevents the while(1) loop from creating a "spin-lock" that would 
         * consume 100% of a CPU core, keeping system overhead near 0%.
         */
        sleep(1);
    }

    /* * CLEANUP
     * Safely release the file descriptor back to the OS. 
     * Note: In a production daemon, this would typically be reached 
     * by intercepting a SIGINT or SIGTERM signal.
     */
    close(fd);
    return 0;
}