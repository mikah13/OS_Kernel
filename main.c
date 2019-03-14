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
	sdCreateFile("meowmeow.txt", GENERIC_READ, 0, 0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);

	hal_io_serial_init();
	hal_io_serial_puts(SerialA, "\nminios: $ ");
	printf("\nminios: $ ");
	uint8_t c;
	char *command;
	command[0] = '\0';
	int len = 0;
	while (1)
	{
		c = hal_io_serial_getc(SerialA);
		if (c == '\n' || c == '\r')
		{
			execute(command);
			hal_io_serial_puts(SerialA, "minios: $ ");
			printf("minios: $ ");
			command[0] = '\0';
		}
		else if (c == '\b')
		{

			if (strlen(command) > 0)
			{
				backspace(command);
				hal_io_serial_puts(SerialA, "\b \b");
				printf("%s", "\b \b");
			}
		}
		else
		{
			concatenate(command, c);
			hal_io_serial_putc(SerialA, c);
			printf("%c", c);
		}
	}

	// DisplayBitmap(743, 624, "./MINIOS.BMP"); //<<<<-- Doesn't seem to work
}
void execute(char *cmd)
{
	char command[2][100];

	int i, j, cnt;
	cnt = 0;
	for (i = 0; i <= (strlen(cmd)); i++)
	{
		// if space or NULL found, assign NULL into splitStrings[cnt]
		if (cmd[i] == ' ' || cmd[i] == '\0')
		{
			command[cnt][j] = '\0';
			cnt++; //for next word
			j = 0; //for next word, init index to 0
		}
		else
		{
			command[cnt][j] = cmd[i];
			j++;
		}
	}

	if (strcmp(command[0], "ls") == 0)
	{

		DisplayDirectory("\\*.*");
		hal_io_serial_puts(SerialA, "\n");
		printf("\n");
	}
	else if (strcmp(command[0], "sysinfo") == 0)
	{
		printf("\nSystem Information: \n");
		sdInitCard(&printf, &printf, true);
		hal_io_serial_puts(SerialA, "\n");
		printf("\n");
	}
	else if (strcmp(command[0], "cat") == 0)
	{
		ReadFile(command[1]);
		hal_io_serial_puts(SerialA, "\n");
		printf("\n");
	}
	else if (strcmp(command[0], "cls") == 0)
	{
		clear_screen();
		hal_io_serial_puts(SerialA, "\n");
	}
	else if (strcmp(command[0], "cd") == 0)
	{
		char *dir = "\\";

		strcat(dir, command[1]);
		strcat(dir, "\\*.*");
		DisplayDirectory(dir);
		hal_io_serial_puts(SerialA, "\n");
		printf("\n");
	}

	else
	{
		printf("\n%s: command not found", command[0]);
		hal_io_serial_puts(SerialA, "\n");
		printf("\n");
	}
}

void clear_screen()
{
	ClearScreen();
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
	if (c >= 1)
	{
		p[c - 1] = '\0';
	}
}
int str_len(char *str)
{
	int c = 0;

	while (str[c] != '\0')
	{
		c++;
	}
	return c;
}
void ReadFile(char *filename)
{
	HANDLE fHandle = sdCreateFile(filename, GENERIC_READ, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	if (fHandle != 0)
	{
		uint32_t bytesRead;

		if ((sdReadFile(fHandle, &buffer[0], 5000, &bytesRead, 0) == true))
		{
			buffer[bytesRead - 1] = '\0'; ///insert null char
			printf("\n%s", &buffer[0]);
		}
		else
		{
			printf("\nFailed to read");
		}

		// Close the file
		sdCloseHandle(fHandle);
	}
	else
	{
		hal_io_serial_puts(SerialA, "\nNo such file or directory");
		printf("\nNo such file or directory");
	}
}
void DisplayDirectory(const char *dirName)
{
	printf("\n");
	unsigned long i = 0;
	HANDLE fh;
	FIND_DATA find;
	char *month[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
	fh = sdFindFirstFile(dirName, &find); // Find first file
	do
	{
		i++;
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