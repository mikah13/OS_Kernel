#include <stdbool.h>                    // Neede for bool
#include <stdint.h>                     // Needed for uint32_t, uint16_t etc
#include <string.h>                     // Needed for memcpy
#include <stdarg.h>                     // Needed for variadic arguments
#include "../drivers/stdio/emb-stdio.h" // Needed for printf
#include "../boot/rpi-smartstart.h"     // Needed for smart start API
#include "../drivers/sdcard/SDCard.h"
#include "../hal/hal.h"

char buffer[500];

void init()
{
    hal_io_video_init();

    hal_io_video_puts("HELLO THERE ", 3, VIDEO_COLOR_WHITE);

    //Typewriter
    hal_io_serial_init();
    hal_io_serial_puts(SerialA, "Typewriter:");
    prompt();
}

void prompt()
{
    uint8_t c hal_io_serial_getc(SerialA);
    uint8_t prevC;
    while (c)
    {
        // c = hal_io_serial_getc(SerialA);
        // if (c == 'l')
        // {
        //     prevC = c;
        // }
        // if (prevC == 'l' && c == 's')
        // {
        //     DisplayDirectory("\\*.*");
        // }
        hal_io_serial_putc(SerialA, c);
        // printf("%c", c);
        if(c == '\n'){
            printf('\n');
        }
        c = hal_io_serial_getc(SerialA);
    }
}

void DisplayDirectory(const char *dirName)
{
    HANDLE fh;
    FIND_DATA find;
    char *month[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    fh = sdFindFirstFile(dirName, &find); // Find first file
    do
    {
        if (find.dwFileAttributes == FILE_ATTRIBUTE_DIRECTORY)
            printf("%s <DIR>\n", find.cFileName);
        else
            printf("%c%c%c%c%c%c%c%c.%c%c%c Size: %9lu bytes, %2d/%s/%4d, LFN: %s\n",
                   find.cAlternateFileName[0], find.cAlternateFileName[1],
                   find.cAlternateFileName[2], find.cAlternateFileName[3],
                   find.cAlternateFileName[4], find.cAlternateFileName[5],
                   find.cAlternateFileName[6], find.cAlternateFileName[7],
                   find.cAlternateFileName[8], find.cAlternateFileName[9],
                   find.cAlternateFileName[10],
                   (unsigned long)find.nFileSizeLow,
                   find.CreateDT.tm_mday, month[find.CreateDT.tm_mon],
                   find.CreateDT.tm_year + 1900,
                   find.cFileName);           // Display each entry
    } while (sdFindNextFile(fh, &find) != 0); // Loop finding next file
    sdFindClose(fh);                          // Close the serach handle
}