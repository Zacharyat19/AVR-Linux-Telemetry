#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <unistd.h>
#include <string.h>
#include <termios.h> // Required for tcdrain() to flush serial buffers

#include "Display.h"

/**
 * HARDWARE CONFIGURATION
 * PHOTO_SERVER_SLOT maps to the first LED (Pin 11) on the Arduino.
 */
#define PHOTO_SERVER_SLOT 0

int main()
{
    /**
     * 1. SERIAL INITIALIZATION
     * Opens the communication channel to the ATmega32U4.
     * Note: Use 'ls -l /dev/ttyACM*' to verify the port if it disconnects.
     */
    int fd = serial_init("/dev/ttyACM0"); 
    if (fd < 0) 
    {
        fprintf(stderr, "Fatal Error: Could not initialize serial port.\n");
        return 1;
    }

    // 0xFF ensures the first loop always triggers a status sync
    uint8_t last_status = 0xFF; 

    while (1)
    {
        /**
         * 2. TELEMETRY: Evaluate State
         * Scans the Linux /proc filesystem for the "Photo-Server" string.
         * This targets the Gunicorn master process or its worker threads.
         */
        uint8_t current_status = 0; // Default to DOWN
        if (process_is_running("Photo-Server")) current_status = 1; // UP

        /**
         * 3. SLOT-BASED TRANSMISSION
         * Only sends data if the status has changed to keep the serial bus quiet.
         * Protocol: 0x10 + Slot (ON) or 0x00 + Slot (OFF)
         */
        if (current_status != last_status)
        {
            uint8_t cmd_byte;
            if (current_status == 1) 
            {
                printf("Telemetry: photo_server UP. Setting Slot %d HIGH.\n", PHOTO_SERVER_SLOT);
                cmd_byte = 0x10 + PHOTO_SERVER_SLOT; 
            } 
            else 
            {
                printf("Telemetry: photo_server DOWN. Setting Slot %d LOW.\n", PHOTO_SERVER_SLOT);
                cmd_byte = 0x00 + PHOTO_SERVER_SLOT; 
            }
            
            // Send the single byte command
            write(fd, &cmd_byte, 1);
            
            // tcdrain() blocks until the byte is physically transmitted out of the UART
            tcdrain(fd); 
            fflush(stdout); 

            last_status = current_status;
        }

        /**
         * 4. ASYNC LISTENER: Monitor for Button 'R'
         * Uses select() to wait for data from the Arduino without blocking the telemetry loop.
         */
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(fd, &read_fds);

        // 1-second timeout acts as the polling interval for telemetry
        struct timeval timeout = {1, 0}; 
        int ready = select(fd + 1, &read_fds, NULL, NULL, &timeout);

        if (ready > 0 && FD_ISSET(fd, &read_fds)) 
        {
            char upstream_cmd;
            // Read the incoming byte from the Arduino
            if (read(fd, &upstream_cmd, 1) > 0) 
            {
                // 'R' is the specific signal sent when Button 1 is pressed
                if (upstream_cmd == 'R') 
                {
                    printf("Hardware Interrupt: Button Pressed. Restarting photo_server...\n");
                    fflush(stdout);
                    
                    // Trigger systemd to restart the service
                    // Note: If running as a non-root user, 'sudo' may be required here
                    system("/bin/systemctl restart photo-server.service");
                    
                    // Force a re-evaluation in the next loop to update the LED immediately
                    last_status = 0xFF; 
                }
            }
        }

        static int loop_counter = 0;

        // Update LCD every 10 iterations (10 seconds)
        if (loop_counter % 10 == 0) 
        {
            char total_msg[32];
            char free_msg[32];
            
            get_storage_strings(total_msg, free_msg, sizeof(total_msg));
            
            // 1. Send Total Space (Line 1 -> Prefix 0x21)
            uint8_t line1_prefix = 0x21;
            write(fd, &line1_prefix, 1);
            write(fd, total_msg, strlen(total_msg));
            tcdrain(fd); // Wait for the transmission to finish
            
            // 2. Send Free Space (Line 2 -> Prefix 0x22)
            uint8_t line2_prefix = 0x22;
            write(fd, &line2_prefix, 1);
            write(fd, free_msg, strlen(free_msg));
            tcdrain(fd);
        }
        loop_counter++;
    }

    close(fd);
    return 0;
}