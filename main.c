/**
*     	main.c
*
*				Kernel's Entry Point
*
*/

#include <stdbool.h>				 // Neede for bool
#include <stdint.h>					 // Needed for uint32_t, uint16_t etc
#include <string.h>					 // Needed for memcpy
#include <stdarg.h>					 // Needed for variadic arguments
#include "drivers/stdio/emb-stdio.h" // Needed for printf
#include "boot/rpi-smartstart.h"	 // Needed for smart start API
#include "drivers/sdcard/SDCard.h"
#include "hal/hal.h"

char buffer[500];

void DisplayDirectory(const char *);

int main(void)
{
	PiConsole_Init(0, 0, 0, &printf); // Auto resolution console, show resolution to screen
	// displaySmartStart(&printf);
	// Display smart start details
	ARM_setmaxspeed(&printf); // ARM CPU to max speed and confirm to screen
	/* Display the SD CARD directory */
	sdInitCard(&printf, &printf, true);

	// printf("\n");
	// printf("Opening Alice.txt \n");

	// HANDLE fHandle = sdCreateFile("Alice.txt", GENERIC_READ, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	// if (fHandle != 0)
	// {
	// 	uint32_t bytesRead;

	// 	if ((sdReadFile(fHandle, &buffer[0], 500, &bytesRead, 0) == true))
	// 	{
	// 		buffer[bytesRead - 1] = '\0'; ///insert null char
	// 		printf("File Contents: %s", &buffer[0]);
	// 	}
	// 	else
	// 	{
	// 		printf("Failed to read");
	// 	}

	// 	// Close the file
	// 	sdCloseHandle(fHandle);
	// }

	hal_io_video_init();

	hal_io_video_puts("HELLO THERE ", 3, VIDEO_COLOR_WHITE);

	//Typewriter
	hal_io_serial_init();
	hal_io_serial_puts(SerialA, "\nminios: $ ");
	printf("%s", "\nminios: $ ");
	uint8_t c;
	uint8_t prevC;
	char *command;
	command[0] = '\0';
	while (1)
	{
		c = hal_io_serial_getc(SerialA);
		// if(c == 'l'){
		// 	 prevC = c;
		// }
		// if(prevC == 'l' && c == 's'){
		// 		DisplayDirectory("\\*.*");
		// }
		// hal_io_serial_putc(SerialA, c);
		// printf("%c", c);
		if (c == '\n' || c == '\r')
		{
			printf("%s", command);
			execute(command);
			hal_io_serial_puts(SerialA, "\nminios: $ ");
			printf("%s", "\nminios: $ ");
			command[0] = '\0';
		}
		else if (c == '\b')
		{

			hal_io_serial_putc(SerialA, c);
			printf("%c", c);
		}
		else
		{
			concatenate(command, c);
			hal_io_serial_putc(SerialA, c);
			printf("%c", c);
		}
	}

	void user_prompt()
	{
	}

	// /* display bitmap on screen */
	// //DisplayBitmap(743, 624, "./MINIOS.BMP");   //<<<<-- Doesn't seem to work

	// while (1){
	// 	set_Activity_LED(1);			// Turn LED on
	// 	timer_wait(500000);				// 0.5 sec delay
	// 	set_Activity_LED(0);			// Turn Led off
	// 	timer_wait(500000);				// 0.5 sec delay
	//   }
	// return(0);
}
void execute(char *cmd)
{
	if (strcmp(cmd, "ls") == 0)
	{

		printf("\nDirectory (/): \n");
		DisplayDirectory("\\*.*");
	}
	else if (strcmp(cmd, "sysinfo") == 0)
	{
		printf("\nSystem Information: \n");
		sdInitCard(&printf, &printf, true);
	}
	else
	{
		printf("\ncommand not found");
	}
}
void concatenate(char *p, char q)
{
	int c = 0;

	while (p[c] != '\0')
	{
		c++;
	}
	p[c] = q;

	c++;

	p[c] = '\0';
}
void backspace(char *p)
{
	int c = 0;

	while (p[c] != '\0')
	{
		c++;
	}
	c--;
	p[0] == '\0';
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
				   find.cFileName);			  // Display each entry
	} while (sdFindNextFile(fh, &find) != 0); // Loop finding next file
	sdFindClose(fh);						  // Close the serach handle
}