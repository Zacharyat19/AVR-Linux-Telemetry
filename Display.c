#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>
#include <stdlib.h>

#include "Display.h"

bool process_is_running(const char* process_name)
{
    DIR* dir = opendir("/proc");
    if(!dir) return false;

    struct dirent* entry;

    while((entry = readdir(dir)) != NULL)
    {
        // Check if the directory name starts with a number (a PID)
        if(isdigit((unsigned char)entry->d_name[0]))
        {
            char buffer[100];
            snprintf(buffer, sizeof(buffer), "/proc/%s/comm", entry->d_name);

            FILE* file = fopen(buffer, "r");
            if(!file) continue;

            char name[50];
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

}

void serial_transmit(int file_descriptor, const uint8_t* payload, size_t size)
{

}