#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>    // For file reading (fopen, fgets, snprintf)
#include <string.h>   // For string comparison (strcmp, strcspn)
#include <dirent.h>   // For directory traversal (opendir, readdir)
#include <ctype.h>    // For character checking (isdigit)
#include <fcntl.h>    // For file control options (open, O_RDWR, etc.)
#include <termios.h>  // For the POSIX terminal control definitions
#include <unistd.h>   // For UNIX standard functions (write, close)

#include "Display.h"

/*
 * KERNEL PROCESS POLLING
 * Instead of spawning a heavy external shell command like system("pgrep"), 
 * this function reads directly from the Linux virtual filesystem (/proc). 
 * This is the highly efficient, standard approach for process monitoring 
 * in embedded C applications.
 */
bool process_is_running(const char* process_name)
{
    // The /proc directory holds all active kernel processes as subdirectories.
    DIR* dir = opendir("/proc");
    if(!dir) return false;

    struct dirent* entry;

    // Iterate through every item in the /proc directory.
    while((entry = readdir(dir)) != NULL)
    {
        // Process directories are strictly numerical (their PID).
        // If the directory name doesn't start with a digit, ignore it.
        if(isdigit((unsigned char)entry->d_name[0]))
        {
            char file_name[280]; 
            // Construct the path to the 'comm' file, which contains the process name.
            snprintf(file_name, sizeof(file_name), "/proc/%s/comm", entry->d_name);

            FILE* file = fopen(file_name, "r");
            if(!file) continue;

            char name[256]; 
            if(fgets(name, sizeof(name), file) != NULL)
            {
                // Strip the trailing newline character injected by the OS.
                name[strcspn(name, "\n")] = '\0';

                // Evaluate if the kernel's process name matches our target.
                if(strcmp(process_name, name) == 0)
                {
                    fclose(file);
                    closedir(dir);
                    return true;
                }
            }
            
            fclose(file); 
        }
    }

    closedir(dir);
    return false;
}

/*
 * HARDWARE SERIAL CONFIGURATION
 * Initializes a POSIX termios interface to communicate with the AVR microcontroller.
 */
int serial_init(const char* device_path)
{
    // O_RDWR: Open for reading and writing.
    // O_NOCTTY: Prevents the USB device from taking over as the Pi's controlling terminal.
    // O_SYNC: Forces write operations to block until physically committed to the hardware.
    int desc = open(device_path, O_RDWR | O_NOCTTY | O_SYNC);
    if(desc < 0) return -1;

    struct termios tty;
    if(tcgetattr(desc, &tty) != 0) 
    {
        close(desc); // Prevent resource leak if attributes fail
        return -1;
    }

    // Set communication speed to 115200 baud for both input and output.
    cfsetospeed(&tty, B115200);
    cfsetispeed(&tty, B115200);

    // 8N1 Mode Setup (8 data bits, No parity, 1 stop bit)
    tty.c_cflag &= ~PARENB; // Clear parity bit
    tty.c_cflag &= ~CSTOPB; // Clear stop field, only one stop bit used
    tty.c_cflag &= ~CSIZE;  // Clear all the size bits
    tty.c_cflag |= CS8;     // 8 bits per byte
    
    // Turn on READ & ignore hardware flow control lines (CLOCAL = 1)
    tty.c_cflag |= CREAD | CLOCAL; 

    // Raw Output Mode Configuration
    // Disables canonical mode (ICANON) and OS-level parsing/newline conversions.
    // This ensures the 0x01 or 0x00 bytes are sent exactly as intended, 
    // without the kernel trying to format them as ASCII text.
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tty.c_oflag &= ~OPOST;

    // Apply the settings to the hardware immediately (TCSANOW)
    if(tcsetattr(desc, TCSANOW, &tty) != 0)
    {
        close(desc);
        return -1;
    }

    // Return the successfully configured file descriptor
    return desc; 
}

/*
 * HARDWARE DISPATCH
 * Pushes the evaluated status byte across the USB bridge.
 */
void serial_transmit(int file_descriptor, uint8_t payload)
{
    write(file_descriptor, &payload, PAYLOAD_SIZE);
}