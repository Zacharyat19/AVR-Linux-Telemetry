#include <stdint.h>
#include <stdbool.h>
#include <sys/statvfs.h>
#include <stdio.h>    // For file reading (fopen, fgets, snprintf)
#include <string.h>   // For string comparison (strcmp, strcspn)
#include <dirent.h>   // For directory traversal (opendir, readdir)
#include <ctype.h>    // For character checking (isdigit)
#include <fcntl.h>    // For file control options (open, O_RDWR, etc.)
#include <termios.h>  // For the POSIX terminal control definitions
#include <unistd.h>   // For UNIX standard functions (write, close)

#include "Display.h"

/**
 * KERNEL PROCESS POLLING
 * Instead of spawning a heavy external shell command like system("pgrep"), 
 * this function reads directly from the Linux virtual filesystem (/proc). 
 * This is the highly efficient, standard approach for process monitoring 
 * in embedded C applications.
 */
int process_is_running(const char* process_name)
{
    // Open the /proc directory, where the kernel stores process information
    DIR* dir = opendir("/proc");
    if (!dir) return 0;

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL)
    {
        // Directories named with numbers represent active Process IDs (PIDs)
        if (isdigit((unsigned char)entry->d_name[0]))
        {
            char path[280];
            // 'cmdline' contains the full execution path and arguments
            snprintf(path, sizeof(path), "/proc/%s/cmdline", entry->d_name);

            FILE* f = fopen(path, "r");
            if (f)
            {
                char buf[1024]; // Larger buffer to capture long paths or venv paths
                size_t len = fread(buf, 1, sizeof(buf) - 1, f);
                if (len > 0)
                {
                    buf[len] = '\0';
                    
                    /**
                     * The kernel separates arguments in /proc/[pid]/cmdline with NULL bytes ('\0').
                     * Standard string functions like strstr() stop at the first NULL. 
                     * We replace NULLs with spaces to treat the whole command line as one searchable string.
                     */
                    for(size_t i = 0; i < len; i++) {
                        if(buf[i] == '\0') buf[i] = ' ';
                    }

                    // Check if our target process name exists anywhere in the command string
                    if (strstr(buf, process_name) != NULL)
                    {
                        fclose(f);
                        closedir(dir);
                        return 1; // Process found
                    }
                }
                fclose(f);
            }
        }
    }
    
    // Cleanup if no match is found after scanning all PIDs
    closedir(dir);
    return 0;
}

/**
 * HARDWARE SERIAL CONFIGURATION
 * Initializes a POSIX termios interface to communicate with the AVR microcontroller.
 */
int serial_init(const char* device_path)
{
    /**
     * O_RDWR: Open for reading and writing.
     * O_NOCTTY: Prevents the USB device from taking over as the Pi's controlling terminal.
     * O_SYNC: Forces write operations to block until physically committed to the hardware.
     */
    int desc = open(device_path, O_RDWR | O_NOCTTY | O_SYNC);
    if(desc < 0) return -1;

    struct termios tty;
    // Get current attributes to use as a baseline
    if(tcgetattr(desc, &tty) != 0) 
    {
        close(desc); // Prevent resource leak if attributes fail
        return -1;
    }

    // Set communication speed to 115200 baud for both input and output.
    cfsetospeed(&tty, B115200);
    cfsetispeed(&tty, B115200);

    /**
     * 8N1 Mode Setup (8 data bits, No parity, 1 stop bit)
     * This is the standard serial configuration for ATmega/Arduino boards.
     */
    tty.c_cflag &= ~PARENB; // Clear parity bit (No parity)
    tty.c_cflag &= ~CSTOPB; // Clear stop field (1 stop bit)
    tty.c_cflag &= ~CSIZE;  // Clear all the size bits
    tty.c_cflag |= CS8;     // Set 8 bits per byte
    
    // CREAD: Enable receiver.
    // CLOCAL: Ignore modem control lines (forces connection without DTR/RTS handshake).
    tty.c_cflag |= CREAD | CLOCAL; 

    /**
     * Raw Output Mode Configuration
     * Disables canonical mode (ICANON) and OS-level parsing/newline conversions.
     * This ensures the 0x10 or 0x00 bytes are sent exactly as intended, 
     * without the kernel trying to format them as ASCII text or adding carriage returns.
     */
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tty.c_oflag &= ~OPOST;

    // Apply the settings to the hardware immediately (TCSANOW)
    if(tcsetattr(desc, TCSANOW, &tty) != 0)
    {
        close(desc);
        return -1;
    }

    // Return the successfully configured file descriptor for use with write() and select()
    return desc; 
}

/**
 * SERIAL TRANSMISSION
 * Encodes the Slot ID and State into a single byte command for the Arduino firmware.
 */
void serial_transmit(int fd, uint8_t slot, uint8_t state)
{
    uint8_t packet;
    
    // Command Protocol: Base Offset + Slot Number
    if (state == 1) {
        packet = CMD_ON + slot;  // e.g., 0x10 for Slot 0 ON
    } 
    else 
    {
        packet = CMD_OFF + slot; // e.g., 0x00 for Slot 0 OFF
    }
    
    // Write the 1-byte command to the serial device
    write(fd, &packet, 1);
    
    /**
     * tcdrain() blocks the calling process until all output written to the file 
     * descriptor has been physically transmitted. This prevents the Pi from 
     * closing the port or moving to the next task before the LED actually flips.
     */
    tcdrain(fd); 
}

/**
 * SYSTEM METRICS
 * Reads storage from statvfs and CPU temperature from the thermal sysfs.
 */
void get_system_metrics(char* line1_buf, char* line2_buf, size_t max_len) 
{
    // 1. Get Disk Space for Line 1
    struct statvfs stat;
    if (statvfs("/", &stat) == 0) 
    {
        double free_gb = (double)(stat.f_bavail * stat.f_frsize) / (1024 * 1024 * 1024);
        snprintf(line1_buf, max_len, "Free:  %.1fGB\n", free_gb); 
    } 
    else 
    {
        snprintf(line1_buf, max_len, "Disk Error\n");
    }

    // 2. Get CPU Temp for Line 2
    FILE* temp_file = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
    if (temp_file) 
    {
        long raw_temp;
        if (fscanf(temp_file, "%ld", &raw_temp) == 1) 
        {
            float celsius = raw_temp / 1000.0;
            snprintf(line2_buf, max_len, "Temp:  %.1fC\n", celsius);
        }
        fclose(temp_file);
    } 
    else 
    {
        snprintf(line2_buf, max_len, "Temp Error\n");
    }
}