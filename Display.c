#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>    // For file reading (fopen, fgets)
#include <string.h>   // For string comparison (strcmp, strcspn)
#include <dirent.h>   // For directory traversal (opendir, readdir)
#include <ctype.h>    // For character checking (isdigit)
#include <stdlib.h>   // For standard utilities
#include <fcntl.h>    // For file control options (O_RDWR, etc.)
#include <termios.h>  // For the POSIX terminal control definitions
#include <unistd.h>   // For UNIX standard functions (write, close)

#include "Display.h"

bool process_is_running(const char* process_name)
{
    DIR* dir = opendir("/proc");
    if(!dir) return false;

    struct dirent* entry;

    while((entry = readdir(dir)) != NULL)
    {
        if(isdigit((unsigned char)entry->d_name[0]))
        {
            char file_name[280]; 
            snprintf(file_name, sizeof(file_name), "/proc/%s/comm", entry->d_name);

            FILE* file = fopen(file_name, "r");
            if(!file) continue;

            char name[256]; 
            if(fgets(name, sizeof(name), file) != NULL)
            {
                name[strcspn(name, "\n")] = '\0';

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

int serial_init(const char* device_path)
{
    int desc = open(device_path, O_RDWR | O_NOCTTY | O_SYNC);
    if(desc < 0) return -1;

    struct termios tty;
    if(tcgetattr(desc, &tty) != 0) 
    {
        close(desc); // Prevent resource leak if attributes fail
        return -1;
    }

    cfsetospeed(&tty, B115200);
    cfsetispeed(&tty, B115200);

    // 8N1 Mode
    tty.c_cflag &= ~PARENB; // Clear parity bit
    tty.c_cflag &= ~CSTOPB; // Clear stop field, only one stop bit used
    tty.c_cflag &= ~CSIZE;  // Clear all the size bits
    tty.c_cflag |= CS8;     // 8 bits per byte
    
    // Turn on READ & ignore ctrl lines (CLOCAL = 1)
    tty.c_cflag |= CREAD | CLOCAL; 

    // Raw Output mode (Disables OS parsing/newline conversions)
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

void serial_transmit(int file_descriptor, uint8_t payload)
{
    write(file_descriptor, &payload, 1);
}