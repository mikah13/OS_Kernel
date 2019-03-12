/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++}
{																			}
{       Filename: rpi-smartstart.c											}
{       Copyright(c): Leon de Boer(LdB) 2017								}
{       Version: 2.02														}
{																			}
{***************[ THIS CODE IS FREEWARE UNDER CC Attribution]***************}
{																            }
{     This sourcecode is released for the purpose to promote programming    }
{  on the Raspberry Pi. You may redistribute it and/or modify with the      }
{  following disclaimer and condition.                                      }
{																            }
{      The SOURCE CODE is distributed "AS IS" WITHOUT WARRANTIES AS TO      }
{   PERFORMANCE OF MERCHANTABILITY WHETHER EXPRESSED OR IMPLIED.            }
{   Redistributions of source code must retain the copyright notices to     }
{   maintain the author credit (attribution) .								}
{																			}
{***************************************************************************}
{                                                                           }
{      This code provides a 32bit or 64bit C wrapper around the assembler   }
{  stub code. In AARCH32 that file is SmartStart32.S, while in AARCH64 the  }
{  file is SmartStart64.S.													}
{	   There file also provides access to the basic hardware of the PI.     }
{  It is also the easy point to insert a couple of important very useful    }
{  Macros that make C development much easier.		        				}
{																            }
{++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

#include <stdbool.h>							// Needed for bool and true/false
#include <stdint.h>								// Needed for uint8_t, uint32_t, uint64_t etc
#include <stdarg.h>								// Needed for variadic arguments
#include <string.h>								// Needed for strlen
#include "Font8x16.h"							// Provides the 8x16 bitmap font for console
#include "rpi-SmartStart.h"						// This units header
#include "../hal/hal.h"

/***************************************************************************}
{       PRIVATE INTERNAL RASPBERRY PI REGISTER STRUCTURE DEFINITIONS        }
****************************************************************************/

/*--------------------------------------------------------------------------}
{    RASPBERRY PI GPIO HARDWARE REGISTERS - BCM2835.PDF Manual Section 6	}
{--------------------------------------------------------------------------*/
struct __attribute__((__packed__, aligned(4))) GPIORegisters {
	volatile uint32_t GPFSEL[6];									// 0x00  GPFSEL0 - GPFSEL5
	uint32_t reserved1;												// 0x18  reserved
	volatile uint32_t GPSET[2];										// 0x1C  GPSET0 - GPSET1;
	uint32_t reserved2;												// 0x24  reserved
	volatile uint32_t GPCLR[2];										// 0x28  GPCLR0 - GPCLR1
	uint32_t reserved3;												// 0x30  reserved
	const volatile uint32_t GPLEV[2];								// 0x34  GPLEV0 - GPLEV1   ** Read only hence const
	uint32_t reserved4;												// 0x3C  reserved
	volatile uint32_t GPEDS[2];										// 0x40  GPEDS0 - GPEDS1
	uint32_t reserved5;												// 0x48  reserved
	volatile uint32_t GPREN[2];										// 0x4C  GPREN0 - GPREN1;
	uint32_t reserved6;												// 0x54  reserved
	volatile uint32_t GPFEN[2];										// 0x58  GPFEN0 - GPFEN1;
	uint32_t reserved7;												// 0x60  reserved
	volatile uint32_t GPHEN[2];										// 0x64  GPHEN0 - GPHEN1;
	uint32_t reserved8;												// 0x6c  reserved
	volatile uint32_t GPLEN[2];										// 0x70  GPLEN0 - GPLEN1;
	uint32_t reserved9;												// 0x78  reserved
	volatile uint32_t GPAREN[2];									// 0x7C  GPAREN0 - GPAREN1;
	uint32_t reserved10;											// 0x84  reserved
	volatile uint32_t GPAFEN[2]; 									// 0x88  GPAFEN0 - GPAFEN1;
	uint32_t reserved11;											// 0x90  reserved
	volatile uint32_t GPPUD; 										// 0x94  GPPUD
	volatile uint32_t GPPUDCLK[2]; 									// 0x98  GPPUDCLK0 - GPPUDCLK1;
};

/*--------------------------------------------------------------------------}
{  RASPBERRY PI SYSTEM TIMER HARDWARE REGISTERS - BCM2835 Manual Section 12	}
{--------------------------------------------------------------------------*/
struct __attribute__((__packed__, aligned(4))) SystemTimerRegisters {
	volatile uint32_t ControlStatus;								// 0x00
	volatile uint32_t TimerLo;										// 0x04
	volatile uint32_t TimerHi;										// 0x08
	volatile uint32_t Compare0;										// 0x0C
	volatile uint32_t Compare1;										// 0x10
	volatile uint32_t Compare2;										// 0x14
	volatile uint32_t Compare3;										// 0x18
};

/*--------------------------------------------------------------------------}
{	   TIMER_CONTROL REGISTER BCM2835 ARM Peripheral manual page 197		}
{--------------------------------------------------------------------------*/
struct __attribute__((__packed__, aligned(4))) TimerControlReg {
	union {
		struct __attribute__((__packed__, aligned(1))) {
			unsigned unused : 1;									// @0 Unused bit
			volatile unsigned Counter32Bit : 1;						// @1 Counter32 bit (16bit if false)
			volatile TIMER_PRESCALE Prescale : 2;					// @2-3 Prescale
			unsigned unused1 : 1;									// @4 Unused bit
			volatile unsigned TimerIrqEnable : 1;					// @5 Timer irq enable
			unsigned unused2 : 1;									// @6 Unused bit
			volatile unsigned TimerEnable : 1;						// @7 Timer enable
			unsigned reserved : 24;									// @8-31 reserved
		};
		uint32_t Raw32;												// Union to access all 32 bits as a uint32_t
	};
};

/*--------------------------------------------------------------------------}
{   RASPBERRY PI ARM TIMER HARDWARE REGISTERS - BCM2835 Manual Section 14	}
{--------------------------------------------------------------------------*/
struct __attribute__((__packed__, aligned(4))) ArmTimerRegisters {
	volatile uint32_t Load;											// 0x00
	const volatile uint32_t Value;									// 0x04  ** Read only hence const
	struct TimerControlReg Control;									// 0x08
	volatile uint32_t Clear;										// 0x0C
	const volatile uint32_t RawIRQ;									// 0x10  ** Read only hence const
	const volatile uint32_t MaskedIRQ;								// 0x14  ** Read only hence const
	volatile uint32_t Reload;										// 0x18
};

/*--------------------------------------------------------------------------}
{   IRQ BASIC PENDING REGISTER - BCM2835.PDF Manual Section 7 page 113/114  }
{--------------------------------------------------------------------------*/
struct __attribute__((__packed__, aligned(4))) IrqBasicPendingReg {
	union {
		struct __attribute__((__packed__, aligned(1))) {
			const volatile unsigned Timer_IRQ_pending : 1;			// @0 Timer Irq pending
			const volatile unsigned Mailbox_IRQ_pending : 1;		// @1 Mailbox Irq pending
			const volatile unsigned Doorbell0_IRQ_pending : 1;		// @2 Arm Doorbell0 Irq pending
			const volatile unsigned Doorbell1_IRQ_pending : 1;		// @3 Arm Doorbell0 Irq pending
			const volatile unsigned GPU0_halted_IRQ_pending : 1;	// @4 GPU0 halted IRQ pending
			const volatile unsigned GPU1_halted_IRQ_pending : 1;	// @5 GPU1 halted IRQ pending
			const volatile unsigned Illegal_access_type1_pending : 1; // @6 Illegal access type 1 IRQ pending
			const volatile unsigned Illegal_access_type0_pending : 1; // @7 Illegal access type 0 IRQ pending
			const volatile unsigned Bits_set_in_pending_register_1 : 1;	// @8 One or more bits set in pending register 1
			const volatile unsigned Bits_set_in_pending_register_2 : 1;	// @9 One or more bits set in pending register 2
			const volatile unsigned GPU_IRQ_7_pending : 1;			// @10 GPU irq 7 pending
			const volatile unsigned GPU_IRQ_9_pending : 1;			// @11 GPU irq 9 pending
			const volatile unsigned GPU_IRQ_10_pending : 1;			// @12 GPU irq 10 pending
			const volatile unsigned GPU_IRQ_18_pending : 1;			// @13 GPU irq 18 pending
			const volatile unsigned GPU_IRQ_19_pending : 1;			// @14 GPU irq 19 pending
			const volatile unsigned GPU_IRQ_53_pending : 1;			// @15 GPU irq 53 pending
			const volatile unsigned GPU_IRQ_54_pending : 1;			// @16 GPU irq 54 pending
			const volatile unsigned GPU_IRQ_55_pending : 1;			// @17 GPU irq 55 pending
			const volatile unsigned GPU_IRQ_56_pending : 1;			// @18 GPU irq 56 pending
			const volatile unsigned GPU_IRQ_57_pending : 1;			// @19 GPU irq 57 pending
			const volatile unsigned GPU_IRQ_62_pending : 1;			// @20 GPU irq 62 pending
			unsigned reserved : 10;									// @21-31 reserved
		};
		const volatile uint32_t Raw32;								// Union to access all 32 bits as a uint32_t
	};
};

/*--------------------------------------------------------------------------}
{	   FIQ CONTROL REGISTER BCM2835.PDF ARM Peripheral manual page 116		}
{--------------------------------------------------------------------------*/
struct __attribute__((__packed__, aligned(4))) FiqControlReg {
	union {
		struct __attribute__((__packed__, aligned(1))) {
			volatile unsigned SelectFIQSource : 7;					// @0-6 Select FIQ source
			volatile unsigned EnableFIQ : 1;						// @7 enable FIQ
			unsigned reserved : 24;									// @8-31 reserved
		};
		volatile uint32_t Raw32;									// Union to access all 32 bits as a uint32_t
	};
};

/*--------------------------------------------------------------------------}
{	 ENABLE BASIC IRQ REGISTER BCM2835 ARM Peripheral manual page 117		}
{--------------------------------------------------------------------------*/
struct __attribute__((__packed__, aligned(4))) EnableBasicIrqReg {
	union {
		struct __attribute__((__packed__, aligned(1))) {
			volatile unsigned Enable_Timer_IRQ : 1;					// @0 Timer Irq enable
			volatile unsigned Enable_Mailbox_IRQ : 1;				// @1 Mailbox Irq enable
			volatile unsigned Enable_Doorbell0_IRQ : 1;				// @2 Arm Doorbell0 Irq enable
			volatile unsigned Enable_Doorbell1_IRQ : 1;				// @3 Arm Doorbell0 Irq enable
			volatile unsigned Enable_GPU0_halted_IRQ : 1;			// @4 GPU0 halted IRQ enable
			volatile unsigned Enable_GPU1_halted_IRQ : 1;			// @5 GPU1 halted IRQ enable
			volatile unsigned Enable_Illegal_access_type1 : 1;		// @6 Illegal access type 1 IRQ enable
			volatile unsigned Enable_Illegal_access_type0 : 1;		// @7 Illegal access type 0 IRQ enable
			unsigned reserved : 24;									// @8-31 reserved
		};
		volatile uint32_t Raw32;									// Union to access all 32 bits as a uint32_t
	};
};

/*--------------------------------------------------------------------------}
{	DISABLE BASIC IRQ REGISTER BCM2835 ARM Peripheral manual page 117		}
{--------------------------------------------------------------------------*/
struct __attribute__((__packed__, aligned(4))) DisableBasicIrqReg {
	union {
		struct __attribute__((__packed__, aligned(1))) {
			volatile unsigned Disable_Timer_IRQ : 1;				// @0 Timer Irq disable
			volatile unsigned Disable_Mailbox_IRQ : 1;				// @1 Mailbox Irq disable
			volatile unsigned Disable_Doorbell0_IRQ : 1;			// @2 Arm Doorbell0 Irq disable
			volatile unsigned Disable_Doorbell1_IRQ : 1;			// @3 Arm Doorbell0 Irq disable
			volatile unsigned Disable_GPU0_halted_IRQ : 1;			// @4 GPU0 halted IRQ disable
			volatile unsigned Disable_GPU1_halted_IRQ : 1;			// @5 GPU1 halted IRQ disable
			volatile unsigned Disable_Illegal_access_type1 : 1;		// @6 Illegal access type 1 IRQ disable
			volatile unsigned Disable_Illegal_access_type0 : 1;		// @7 Illegal access type 0 IRQ disable
			unsigned reserved : 24;									// @8-31 reserved
		};
		volatile uint32_t Raw32;									// Union to access all 32 bits as a uint32_t
	};
};

/*--------------------------------------------------------------------------}
{	   RASPBERRY PI IRQ HARDWARE REGISTERS - BCM2835 Manual Section 7	    }
{--------------------------------------------------------------------------*/
struct __attribute__((__packed__, aligned(4))) IrqControlRegisters {
	const volatile struct IrqBasicPendingReg IRQBasicPending;		// 0x200   ** Read only hence const
	volatile uint32_t IRQPending1;									// 0x204
	volatile uint32_t IRQPending2;									// 0x208
	volatile struct FiqControlReg FIQControl;						// 0x20C
	volatile uint32_t EnableIRQs1;									// 0x210
	volatile uint32_t EnableIRQs2;									// 0x214
	volatile struct EnableBasicIrqReg EnableBasicIRQs;				// 0x218
	volatile uint32_t DisableIRQs1;									// 0x21C
	volatile uint32_t DisableIRQs2;									// 0x220
	volatile struct DisableBasicIrqReg DisableBasicIRQs;			// 0x224
};

/*--------------------------------------------------------------------------}
;{               RASPBERRY PI MAILBOX HARRDWARE REGISTERS					}
;{-------------------------------------------------------------------------*/
struct __attribute__((__packed__, aligned(4))) MailBoxRegisters {
	const volatile uint32_t Read0;									// 0x00         Read data from VC to ARM
	uint32_t Unused[3];												// 0x04-0x0F
	volatile uint32_t Peek0;										// 0x10
	volatile uint32_t Sender0;										// 0x14
	volatile uint32_t Status0;										// 0x18         Status of VC to ARM
	volatile uint32_t Config0;										// 0x1C
	volatile uint32_t Write1;										// 0x20         Write data from ARM to VC
	uint32_t Unused2[3];											// 0x24-0x2F
	volatile uint32_t Peek1;										// 0x30
	volatile uint32_t Sender1;										// 0x34
	volatile uint32_t Status1;										// 0x38         Status of ARM to VC
	volatile uint32_t Config1;										// 0x3C
};

/***************************************************************************}
{        PRIVATE INTERNAL RASPBERRY PI REGISTER STRUCTURE CHECKS            }
****************************************************************************/

/*--------------------------------------------------------------------------}
{					 CODE TYPE STRUCTURE COMPILE TIME CHECKS	            }
{--------------------------------------------------------------------------*/
/* If you have never seen compile time assertions it's worth google search */
/* on "Compile Time Assertions". It is part of the C11++ specification and */
/* all compilers that support the standard will have them (GCC, MSC inc)   */
/*-------------------------------------------------------------------------*/
#include <assert.h>								// Need for compile time static_assert

/* Check the code type structure size */
static_assert(sizeof(CODE_TYPE) == 0x04, "Structure CODE_TYPE should be 0x04 bytes in size");
static_assert(sizeof(CPU_ID) == 0x04, "Structure CPU_ID should be 0x04 bytes in size");
static_assert(sizeof(struct GPIORegisters) == 0xA0, "Structure GPIORegisters should be 0xA0 bytes in size");
static_assert(sizeof(struct SystemTimerRegisters) == 0x1C, "Structure SystemTimerRegisters should be 0x1C bytes in size");
static_assert(sizeof(struct ArmTimerRegisters) == 0x1C, "Structure ArmTimerRegisters should be 0x1C bytes in size");
static_assert(sizeof(struct IrqControlRegisters) == 0x28, "Structure IrqControlRegisters should be 0x28 bytes in size");
static_assert(sizeof(struct MailBoxRegisters) == 0x40, "Structure MailBoxRegisters should be 0x40 bytes in size");

/***************************************************************************}
{     PUBLIC POINTERS TO ALL OUR RASPBERRY PI REGISTER BANK STRUCTURES	    }
****************************************************************************/
#define GPIO ((volatile __attribute__((aligned(4))) struct GPIORegisters*)(uintptr_t)(RPi_IO_Base_Addr + 0x200000))
#define SYSTEMTIMER ((volatile __attribute__((aligned(4))) struct SystemTimerRegisters*)(uintptr_t)(RPi_IO_Base_Addr + 0x3000))
#define IRQ ((volatile __attribute__((aligned(4))) struct IrqControlRegisters*)(uintptr_t)(RPi_IO_Base_Addr + 0xB200))
#define ARMTIMER ((volatile __attribute__((aligned(4))) struct  ArmTimerRegisters*)(uintptr_t)(RPi_IO_Base_Addr + 0xB400))
#define MAILBOX ((volatile __attribute__((aligned(4))) struct MailBoxRegisters*)(uintptr_t)(RPi_IO_Base_Addr + 0xB880))


/*--------------------------------------------------------------------------}
{						  INTERNAL IMAGE PTR UNION							}
{--------------------------------------------------------------------------*/
typedef struct __attribute__((__packed__, aligned(4))) tagIMAGE_PTR {
	union {
		uint8_t* rawImage;
		RGB565* __attribute__((__packed__, aligned(2))) ptrRGB565;
		RGB* __attribute__((__packed__, aligned(1))) ptrRGB;
		RGBA* __attribute__((__packed__, aligned(4))) ptrRGBA;
	};
} IMAGE_PTR;

/*--------------------------------------------------------------------------}
{						  INTERNAL DC  STRUCTURE							}
{--------------------------------------------------------------------------*/
typedef struct __attribute__((__packed__, aligned(4))) tagINTDC {
	uintptr_t fb;													// Frame buffer address
	uint32_t wth;													// Screen width (of frame buffer)
	uint32_t ht;													// Screen height (of frame buffer)
	uint32_t depth;													// Colour depth (of frame buffer)
																	/* Position control */
	POINT curPos;													// Current position
	POINT cursor;													// Current cursor position

																	/* Text colour control */
	RGBA TxtColor;													// Text colour to write
	RGBA BkColor;													// Background colour to write
	RGBA BrushColor;												// Brush colour to write

	void(*ClearArea) (struct tagINTDC* dc, uint_fast32_t x1, uint_fast32_t y1, uint_fast32_t x2, uint_fast32_t y2);
	void(*VertLine) (struct tagINTDC* dc, uint_fast32_t cy, int_fast8_t dir);
	void(*HorzLine) (struct tagINTDC* dc, uint_fast32_t cx, int_fast8_t dir);
	void(*DiagLine) (struct tagINTDC* dc, uint_fast32_t dx, uint_fast32_t dy, int_fast8_t xdir, int_fast8_t ydir);
	void(*WriteChar) (struct tagINTDC* dc, uint8_t Ch);
	void(*PutImage) (struct tagINTDC* dc, uint_fast32_t dx, uint_fast32_t dy, IMAGE_PTR imgSrc, bool BottomUp);
} INTDC;


INTDC __attribute__((aligned(4))) console = { 0 };

/***************************************************************************}
{                       PUBLIC C INTERFACE ROUTINES                         }
{***************************************************************************/

/*==========================================================================}
{						   PUBLIC GPIO ROUTINES								}
{==========================================================================*/

/*-[gpio_setup]-------------------------------------------------------------}
. Given a valid GPIO port number and mode sets GPIO to given mode
. RETURN: true for success, false for any failure
. 30Jun17 LdB
.--------------------------------------------------------------------------*/
bool gpio_setup (uint_fast8_t gpio, GPIOMODE mode) {
	if (gpio > 54) return false;									// Check GPIO pin number valid, return false if invalid
	if (mode < 0 || mode > GPIO_ALTFUNC3) return false;				// Check requested mode is valid, return false if invalid
	uint_fast32_t bit = ((gpio % 10) * 3);							// Create bit mask
	uint32_t mem = GPIO->GPFSEL[gpio / 10];							// Read register
	mem &= ~(7 << bit);												// Clear GPIO mode bits for that port
	mem |= (mode << bit);											// Logical OR GPIO mode bits
	GPIO->GPFSEL[gpio / 10] = mem;									// Write value to register
	return true;													// Return true
}

/*-[gpio_output]------------------------------------------------------------}
. Given a valid GPIO port number the output is set high(true) or Low (false)
. RETURN: true for success, false for any failure
. 30Jun17 LdB
.--------------------------------------------------------------------------*/
bool gpio_output (uint_fast8_t gpio, bool on) {
	if (gpio > 54) return false;									// Check GPIO pin number valid, return false if invalid
	uint_fast32_t bit = 1 << (gpio % 32);							// Create mask bit
	if (on) {														// ON request
		GPIO->GPSET[gpio / 32] = bit;								// Set bit to make GPIO high output
	} else {
		GPIO->GPCLR[gpio / 32] = bit;								// Set bit to make GPIO low output
	}
	return true;													// Return true
}

/*-[gpio_input]-------------------------------------------------------------}
. Reads the actual level of the GPIO port number
. RETURN: true = GPIO input high, false = GPIO input low
. 30Jun17 LdB
.--------------------------------------------------------------------------*/
bool gpio_input (uint_fast8_t gpio) {
	if (gpio > 54) return false;									// Check GPIO pin number valid, return false if invalid
	uint_fast32_t bit = 1 << (gpio % 32);							// Create mask bit
	uint32_t mem = GPIO->GPLEV[gpio / 32];							// Read port level
	if (mem & bit) return true;										// Return true if bit set
	return false;													// Return false
}

/*-[gpio_checkEvent]-------------------------------------------------------}
. Checks the given GPIO port number for an event/irq flag.
. RETURN: true for event occured, false for no event
. 30Jun17 LdB
.-------------------------------------------------------------------------*/
bool gpio_checkEvent (uint_fast8_t gpio) {
	if (gpio > 54) return false;									// Check GPIO pin number valid, return false if invalid
	uint_fast32_t bit = 1 << (gpio % 32);							// Create mask bit
	uint32_t mem = GPIO->GPEDS[gpio / 32];							// Read event detect status register
	if (mem & bit) return true;										// Return true if bit set
	return false;													// Return false
}

/*-[gpio_clearEvent]-------------------------------------------------------}
. Clears the given GPIO port number event/irq flag.
. RETURN: true for success, false for any failure
. 30Jun17 LdB
.-------------------------------------------------------------------------*/
bool gpio_clearEvent (uint_fast8_t gpio) {
	if (gpio > 54) return false;									// Check GPIO pin number valid, return false if invalid
	uint_fast32_t bit = 1 << (gpio % 32);							// Create mask bit
	GPIO->GPEDS[gpio / 32] = bit;									// Clear the event from GPIO register
	return true;													// Return true
}

/*-[gpio_edgeDetect]-------------------------------------------------------}
. Sets GPIO port number edge detection to lifting/falling in Async/Sync mode
. RETURN: true for success, false for any failure
. 30Jun17 LdB
.-------------------------------------------------------------------------*/
bool gpio_edgeDetect (uint_fast8_t gpio, bool lifting, bool Async) {
	if (gpio > 54) return false;									// Check GPIO pin number valid, return false if invalid
	uint_fast32_t bit = 1 << (gpio % 32);							// Create mask bit
	if (lifting) {													// Lifting edge detect
		if (Async) GPIO->GPAREN[gpio / 32] = bit;					// Asynchronous lifting edge detect register bit set
			else GPIO->GPREN[gpio / 32] = bit;						// Synchronous lifting edge detect register bit set
	} else {														// Falling edge detect
		if (Async) GPIO->GPAFEN[gpio / 32] = bit;					// Asynchronous falling edge detect register bit set
			else GPIO->GPFEN[gpio / 32] = bit;						// Synchronous falling edge detect register bit set
	}
	return true;													// Return true
}

/*-[gpio_fixResistor]------------------------------------------------------}
. Set the GPIO port number with fix resistors to pull up/pull down.
. RETURN: true for success, false for any failure
. 30Jun17 LdB
.-------------------------------------------------------------------------*/
bool gpio_fixResistor (uint_fast8_t gpio, GPIO_FIX_RESISTOR resistor) {
	uint64_t endTime;
	if (gpio > 54) return false;									// Check GPIO pin number valid, return false if invalid
	if (resistor < 0 || resistor > PULLDOWN) return false;			// Check requested resistor is valid, return false if invalid
	GPIO->GPPUD = resistor;											// Set fixed resistor request to PUD register
	endTime = timer_getTickCount() + 2;								// We want a 2 usec delay
	while (timer_getTickCount() < endTime) {}						// Wait for the timeout
	uint_fast32_t bit = 1 << (gpio % 32);							// Create mask bit
	GPIO->GPPUDCLK[gpio / 32] = bit;								// Set the PUD clock bit register
	endTime = timer_getTickCount() + 2;								// We want a 2 usec delay
	while (timer_getTickCount() < endTime) {}						// Wait for the timeout
	GPIO->GPPUD = 0;												// Clear GPIO resister setting
	GPIO->GPPUDCLK[gpio / 32] = 0;									// Clear PUDCLK from GPIO
	return true;													// Return true
}

/*==========================================================================}
{						   PUBLIC TIMER ROUTINES							}
{==========================================================================*/

/*-[timer_getTickCount]-----------------------------------------------------}
. Get 1Mhz ARM system timer tick count in full 64 bit.
. The timer read is as per the Broadcom specification of two 32bit reads
. RETURN: tickcount value as an unsigned 64bit value
. 30Jun17 LdB
.--------------------------------------------------------------------------*/
uint64_t timer_getTickCount (void) {
	uint64_t resVal;
	uint32_t lowCount;
	do {
		resVal = SYSTEMTIMER->TimerHi; 								// Read Arm system timer high count
		lowCount = SYSTEMTIMER->TimerLo;							// Read Arm system timer low count
	} while (resVal != (uint64_t)SYSTEMTIMER->TimerHi);				// Check hi counter hasn't rolled in that time
	resVal = (uint64_t)resVal << 32 | lowCount;						// Join the 32 bit values to a full 64 bit
	return(resVal);													// Return the uint64_t timer tick count
}

/*-[timer_Wait]-------------------------------------------------------------}
. This will simply wait the requested number of microseconds before return.
. 02Jul17 LdB
.--------------------------------------------------------------------------*/
void timer_wait (uint64_t us) {
	us += timer_getTickCount();										// Add current tickcount onto delay
	while (timer_getTickCount() < us) {};							// Loop on timeout function until timeout
}


/*-[tick_Difference]--------------------------------------------------------}
. Given two timer tick results it returns the time difference between them.
. 02Jul17 LdB
.--------------------------------------------------------------------------*/
uint64_t tick_difference (uint64_t us1, uint64_t us2) {
	if (us1 > us2) {												// If timer one is greater than two then timer rolled
		uint64_t td = UINT64_MAX - us1 + 1;							// Counts left to roll value
		return us2 + td;											// Add that to new count
	}
	return us2 - us1;												// Return difference between values
}

/*==========================================================================}
{					     PUBLIC PI MAILBOX ROUTINES							}
{==========================================================================*/
#define MAIL_EMPTY	0x40000000		/* Mailbox Status Register: Mailbox Empty */
#define MAIL_FULL	0x80000000		/* Mailbox Status Register: Mailbox Full  */

/*-[mailbox_write]----------------------------------------------------------}
. This will execute the sending of the given data block message thru the
. mailbox system on the given channel.
. RETURN: True for success, False for failure.
. 04Jul17 LdB
.--------------------------------------------------------------------------*/
bool mailbox_write (MAILBOX_CHANNEL channel, uint32_t message) {
	uint32_t value;													// Temporary read value
	if (channel > MB_CHANNEL_GPU)  return false;					// Channel error
	message &= ~(0xF);												// Make sure 4 low channel bits are clear
	message |= channel;												// OR the channel bits to the value
	do {
		value = MAILBOX->Status1;									// Read mailbox1 status from GPU
	} while ((value & MAIL_FULL) != 0);								// Make sure arm mailbox is not full
	MAILBOX->Write1 = message;										// Write value to mailbox
	return true;													// Write success
}

/*-[mailbox_read]-----------------------------------------------------------}
. This will read any pending data on the mailbox system on the given channel.
. RETURN: The read value for success, 0xFEEDDEAD for failure.
. 04Jul17 LdB
.--------------------------------------------------------------------------*/
uint32_t mailbox_read (MAILBOX_CHANNEL channel) {
	uint32_t value;													// Temporary read value
	if (channel > MB_CHANNEL_GPU)  return 0xFEEDDEAD;				// Channel error
	do {
		do {
			value = MAILBOX->Status0;								// Read mailbox0 status
		} while ((value & MAIL_EMPTY) != 0);						// Wait for data in mailbox
		value = MAILBOX->Read0;										// Read the mailbox
	} while ((value & 0xF) != channel);								// We have response back
	value &= ~(0xF);												// Lower 4 low channel bits are not part of message
	return value;													// Return the value
}

/*-[mailbox_tag_message]----------------------------------------------------}
. This will post and execute the given variadic data onto the tags channel
. on the mailbox system. You must provide the correct number of response
. uint32_t variables and a pointer to the response buffer. You nominate the
. number of data uint32_t for the call and fill the variadic data in. If you
. do not want the response data back the use NULL for response_buf pointer.
. RETURN: True for success and the response data will be set with data
.         False for failure and the response buffer is untouched.
. 04Jul17 LdB
.--------------------------------------------------------------------------*/
bool mailbox_tag_message (uint32_t* response_buf,					// Pointer to response buffer
						  uint8_t data_count,						// Number of uint32_t data following
						  ...)										// Variadic uint32_t values for call
{
	uint32_t __attribute__((aligned(16))) message[32];
	va_list list;
	va_start(list, data_count);										// Start variadic argument
	message[0] = (data_count + 3) * 4;								// Size of message needed
	message[data_count + 2] = 0;									// Set end pointer to zero
	message[1] = 0;													// Zero response message
	for (int i = 0; i < data_count; i++) {
		message[2 + i] = va_arg(list, uint32_t);					// Fetch next variadic
	}
	va_end(list);													// variadic cleanup
	mailbox_write(MB_CHANNEL_TAGS, ARMaddrToGPUaddr(&message[0]));	// Write message to mailbox
	mailbox_read(MB_CHANNEL_TAGS);									// Wait for write response
	if (message[1] == 0x80000000) {
		if (response_buf) {											// If buffer NULL used then don't want response
			for (int i = 0; i < data_count; i++)
				response_buf[i] = message[2 + i];					// Transfer out each response message
		}
		return true;												// message success
	}
	return false;													// Message failed
}

/*==========================================================================}
{				     PUBLIC PI ACTIVITY LED ROUTINES						}
{==========================================================================*/
static bool ModelCheckHasRun = false;								// Flag set if model check has run
static uint_fast8_t ActivityGPIOPort = 47;							// Default GPIO for activity led is 47

/*-[set_Activity_LED]-------------------------------------------------------}
. This will set the PI activity LED on or off as requested. The SmartStart
. stub provides the Pi board autodetection so the right GPIO port is used.
. RETURN: True the LED state was successfully change, false otherwise
. 04Jul17 LdB
.--------------------------------------------------------------------------*/
bool set_Activity_LED (bool on) {
	switch (RPi_CpuId.PartNumber) {
		case 0xb76: {												// arm1176jzf-s AKA Pi1
			if (!ModelCheckHasRun) {								// Just check board isn't early Pi1 on GPIO16
				uint32_t model[4];
				ModelCheckHasRun = true;							// Set we have run the model check
				if (mailbox_tag_message(&model[0], 4,
					MAILBOX_TAG_GET_BOARD_REVISION, 4, 0, 0)) {
					if ((model[3] >= 0x0002) && (model[3] <= 0x000F))
					{												// Models A, B return 0002 to 000F
						ActivityGPIOPort = 16;						// GPIO port 16 is activity led
					} else ActivityGPIOPort = 47;					// GPIO port 47 activity as default
				}
			}
			gpio_output(ActivityGPIOPort, on);						// GPIO activity (port 16 or 47 depends on model) on/off
			return true;											// Return true
		}
		case 0xc07: {												// cortex-a7 AKA Pi2
			gpio_output(47, on);									// GPIO port 47 on/off
			return true;											// Return true
		}
		case 0xd03: {												// cortex-a53 AKA Pi3
			return (mailbox_tag_message(0, 5,
				MAILBOX_TAG_SET_GPIO_STATE,
				8, 8, 130, (uint32_t)on));							// Mailbox message,set GPIO port 130, on/off
		}
	}
	return false;													// Return false if above fail
}

/*==========================================================================}
{				     PUBLIC ARM CPU SPEED SET ROUTINES						}
{==========================================================================*/

/*-[ARM_setmaxspeed]--------------------------------------------------------}
. This will set the ARM cpu to the maximum. You can optionally print confirm
. message to screen but providing a print handler.
. RETURN: True maxium speed was successfully set, false otherwise
. 04Jul17 LdB
.--------------------------------------------------------------------------*/
bool ARM_setmaxspeed (printhandler prn_handler) {
	uint32_t Buffer[5];
	if (mailbox_tag_message(&Buffer[0], 5, MAILBOX_TAG_GET_MAX_CLOCK_RATE, 8, 8, 3, 0))
		if (mailbox_tag_message(&Buffer[0], 5, MAILBOX_TAG_SET_CLOCK_RATE, 8, 8, 3, Buffer[4])) {
			if (prn_handler) prn_handler("CPU frequency set to %u Hz\n", Buffer[4]);
			return true;											// Return success
		}
	return false;													// Max speed set failed
}

/*==========================================================================}
{				      SMARTSTART DISPLAY ROUTINES							}
{==========================================================================*/

/*-[displaySmartStart]------------------------------------------------------}
. This will print 2 lines of basic smart start details to given print handler
. 04Jul17 LdB
.--------------------------------------------------------------------------*/
void displaySmartStart (printhandler prn_handler) {
	if (prn_handler) {
		prn_handler("SmartStart v2.02 compiled for Arm%d, AARCH%d with %u core s/w support\n",
			RPi_CompileMode.ArmCodeTarget, RPi_CompileMode.AArchMode * 32 + 32,
			(unsigned int)RPi_CompileMode.CoresSupported);							// Write text
		prn_handler("Detected %s CPU, part id: 0x%03X, Cores made ready for use: %u\n",
			RPi_CpuIdString(), RPi_CpuId.PartNumber, (unsigned int)RPi_CoresReady); // Write text
	}
}





/*-Embedded_Console_WriteChar-----------------------------------------------}
. Writes the given character to the console and preforms cursor movements as
. required by what the character is.
. 25Nov16 LdB
.--------------------------------------------------------------------------*/
void Embedded_Console_WriteChar(char Ch) {
	switch (Ch) {
	case '\r': {											// Carriage return character
		console.cursor.x = 0;								// Cursor back to line start
	}
			   break;
	case '\t': {											// Tab character character
		console.cursor.x += 5;								// Cursor increment to by 5
		console.cursor.x -= (console.cursor.x % 4);			// align it to 4
	}
			   break;
	case '\n': {											// New line character
		console.cursor.x = 0;								// Cursor back to line start
		console.cursor.y++;									// Increment cursor down a line
	}
			   break;
	default: {												// All other characters
		console.curPos.x = console.cursor.x * BitFontWth;
		console.curPos.y = console.cursor.y * BitFontHt;
		console.WriteChar(&console, Ch);					// Write the character to graphics screen
		console.cursor.x++;									// Cursor.x forward one character
	}
			 break;
	}
}

/*-WriteText-----------------------------------------------------------------
Draws given string in BitFont Characters in the colour specified starting at
(X,Y) on the screen. It just redirects each character write to WriteChar.
25Nov16 LdB
--------------------------------------------------------------------------*/
void WriteText(int X, int Y, char* Txt) {
	if (Txt) {														// Check pointer valid
		console.cursor.x = X;										// Set cursor x position
		console.cursor.y = Y;										// Set cursor y position
		size_t count = strlen(Txt);									// Fetch string length
		while (count) {												// For each character
			Embedded_Console_WriteChar(Txt[0]);						// Write the character
			count--;												// Decrement count
			Txt++;													// Next text character
		}
	}
}





COLORREF SetDCPenColor(HDC hdc,								// Handle to the DC
	COLORREF crColor)						// The new pen color
{
	COLORREF retValue = 0;
	INTDC* intDC = (INTDC*)hdc;										// Typecast hdc to internal DC
	if (intDC) {
		retValue = intDC->TxtColor.ref;								// We will return current text color
		intDC->TxtColor.ref = crColor;								// Update text color
	}
	return (retValue);												// Return color reference
}

COLORREF SetDCBrushColor(HDC hdc,								// Handle to the DC
	COLORREF crColor)					// The new brush color
{
	COLORREF retValue = 0;
	INTDC* intDC = (INTDC*)hdc;										// Typecast hdc to internal DC
	if (intDC) {
		retValue = intDC->BrushColor.ref;							// We will return current brush color
		intDC->BrushColor.ref = crColor;							// Update brush colour
	}
	return (retValue);												// Return color reference
}

BOOL MoveToEx(HDC hdc,
	int_fast32_t X,
	int_fast32_t Y,
	POINT* lpPoint)
{
	INTDC* intDC = (INTDC*)hdc;										// Typecast hdc to internal DC
	if (intDC) {													// Pointer valid
		if (lpPoint) *lpPoint = intDC->curPos;						// Return current position
		intDC->curPos.x = X;										// Update x position
		intDC->curPos.y = Y;										// Update y position
		return TRUE;
	}
	return FALSE;
}

BOOL LineTo(HDC hdc,
	int nXEnd,
	int nYEnd)
{
	INTDC* intDC = (INTDC*)hdc;										// Typecast hdc to internal DC
	if (intDC) {													// Pointer valid
		int_fast8_t xdir, ydir;
		uint_fast32_t dx, dy;
		if (nXEnd < intDC->curPos.x) {
			dx = intDC->curPos.x - nXEnd;
			xdir = -1;
		}
		else {
			dx = nXEnd - intDC->curPos.x;
			xdir = 1;
		}
		if (nYEnd < intDC->curPos.y) {
			dy = intDC->curPos.y - nYEnd;
			ydir = -1;
		}
		else {
			dy = nYEnd - intDC->curPos.y;
			ydir = 1;
		}
		if (dx == 0) intDC->VertLine(intDC, dy, ydir);
		else if (dy == 0) intDC->HorzLine(intDC, dx, xdir);
		else intDC->DiagLine(intDC, dx, dy, xdir, ydir);
		return TRUE;
	}
	return FALSE;
}


BOOL TextOut(HDC hdc,
	int_fast32_t nXStart,
	int_fast32_t nYStart,
	const TCHAR* lpString,
	int_fast32_t cchString)
{
	if ((cchString) && (lpString)) {								// Check text data valid
		for (int_fast32_t i = 0; i < cchString; i++) {
			console.curPos.x = nXStart;
			console.curPos.y = nYStart;
			console.WriteChar(&console, lpString[i]);				// Write the character
			nXStart += BitFontWth;									// Advance x text position
		}
		return TRUE;
	}
	return FALSE;
}


BOOL BmpOut(HDC hdc,
	uint32_t nXStart,
	uint32_t nYStart,
	uint32_t cX,
	uint32_t cY,
	uint8_t* imgSrc)
{
	INTDC* intDC = (INTDC*)hdc;										// Typecast hdc to internal DC
	if (intDC) {													// Pointer valid

		IMAGE_PTR p;
		//p.rawImage = &imgSrc[sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER)];
		p.rawImage = &imgSrc[0];
		intDC->curPos.x = nXStart;
		intDC->curPos.y = nYStart + cY;
		intDC->PutImage(intDC, cX, cY, p, true);
		return TRUE;
	}
	return FALSE;
}

BOOL CvtBmpLine (HDC hdc,
	uint32_t nXStart,
	uint32_t nYStart,
	uint32_t cX,
	uint32_t imgDepth,
	uint8_t* imgSrc)
{
	IMAGE_PTR p;
	uint8_t __attribute__((aligned(4))) buffer[4096];
	p.rawImage = &buffer[0];
	INTDC* intDC = (INTDC*)hdc;										// Typecast hdc to internal DC
	if (intDC) {													// Pointer valid
		intDC->curPos.x = nXStart;
		intDC->curPos.y = nYStart;
		if (intDC->depth != imgDepth) {
			switch (intDC->depth) {
				case 16:
					if (imgDepth == 24) {
						RGB val;
						RGB565 out;
						RGB565* __attribute__((aligned(2))) video_wr_ptrD = (RGB565*)(uintptr_t)&buffer[0];
						RGB* __attribute__((aligned(1))) video_wr_ptrS = (RGB*)(uintptr_t)&imgSrc[0];
						for (unsigned int i = 0; i < cX; i++) {
							val = *video_wr_ptrS++;
							out.R = val.rgbRed >> 3;
							out.G =  val.rgbGreen >> 2;
							out.B = val.rgbBlue >> 3;
							*video_wr_ptrD++ = out;
						}
					} else {
						RGBA val;
						RGB565 out;
						RGB565* __attribute__((aligned(2))) video_wr_ptrD = (RGB565*)(uintptr_t)&buffer[0];
						RGBA* __attribute__((aligned(4))) video_wr_ptrS = (RGBA*)(uintptr_t)&imgSrc[0];
						for (unsigned int i = 0; i < cX; i++) {
							val = *video_wr_ptrS++;
							out.R = val.rgbRed >> 3;
							out.G = val.rgbGreen >> 2;
							out.B = val.rgbBlue >> 3;
							*video_wr_ptrD++ = out;
						}
					}
					break;
				case 24:
					if (imgDepth == 16) {
						RGB565 val;
						RGB out;
						RGB* __attribute__((aligned(1))) video_wr_ptrD = (RGB*)(uintptr_t)&buffer[0];
						RGB565* __attribute__((aligned(2))) video_wr_ptrS = (RGB565*)(uintptr_t)&imgSrc[0];
						for (unsigned int i = 0; i < cX; i++) {
							val = *video_wr_ptrS++;
							out.rgbRed = val.R << 3;
							out.rgbGreen = val.G << 2;
							out.rgbBlue = val.B << 3;
							*video_wr_ptrD++ = out;
						}
					} else {
						RGBA val;
						RGB out;
						RGB* __attribute__((aligned(1))) video_wr_ptrD = (RGB*)(uintptr_t)&buffer[0];
						RGBA* __attribute__((aligned(4))) video_wr_ptrS = (RGBA*)(uintptr_t)&imgSrc[0];
						for (unsigned int i = 0; i < cX; i++) {
							val = *video_wr_ptrS++;
							out.rgbRed = val.rgbRed;
							out.rgbGreen = val.rgbGreen;
							out.rgbBlue = val.rgbBlue;
							*video_wr_ptrD++ = out;
						}
					}
					break;
				case 32:
					if (imgDepth == 16) {
						RGB565 val;
						RGBA out;
						RGBA* __attribute__((aligned(4))) video_wr_ptrD = (RGBA*)(uintptr_t)&buffer[0];
						RGB565* __attribute__((aligned(2))) video_wr_ptrS = (RGB565*)(uintptr_t)&imgSrc[0];
						for (unsigned int i = 0; i < cX; i++) {
							val = *video_wr_ptrS++;
							out.rgbRed = val.R << 3;
							out.rgbGreen = val.G << 2;
							out.rgbBlue = val.B << 3;
							*video_wr_ptrD++ = out;
						}
					}
					else {
						RGB val;
						RGBA out;
						RGBA* __attribute__((aligned(4))) video_wr_ptrD = (RGBA*)(uintptr_t)&buffer[0];
						RGB* __attribute__((aligned(1))) video_wr_ptrS = (RGB*)(uintptr_t)&imgSrc[0];
						for (unsigned int i = 0; i < cX; i++) {
							val = *video_wr_ptrS++;
							out.rgbRed = val.rgbRed;
							out.rgbGreen = val.rgbGreen;
							out.rgbBlue = val.rgbBlue;
							*video_wr_ptrD++ = out;
						}
					}
					break;
			}
		}
		intDC->PutImage(intDC, cX, 1, p, true);
		return TRUE;
	}
	return FALSE;
}





/*--------------------------------------------------------------------------}
{					   16 BIT COLOUR GRAPHICS ROUTINES						}
{--------------------------------------------------------------------------*/

/*-[INTERNAL: ClearArea16]--------------------------------------------------}
. 16 Bit colour version of the clear area call which block fills the given
. area from (x1,y1) to (x2,y2) with the current brush colour. As an internal
. function the pairs are assumed to be correvtly ordered and dc valid.
. 10Aug17 LdB
.--------------------------------------------------------------------------*/
static void ClearArea16 (INTDC* dc, uint_fast32_t x1, uint_fast32_t y1, uint_fast32_t x2, uint_fast32_t y2) {
	RGB565* __attribute__((__packed__, aligned(2))) video_wr_ptr = (RGB565*)(uintptr_t)(dc->fb + (y1 * dc->wth * 2) + (x1 * 2));
	RGB565 Bc;
	Bc.R = dc->BrushColor.rgbRed >> 3;
	Bc.G = dc->BrushColor.rgbGreen >> 2;
	Bc.B = dc->BrushColor.rgbBlue >> 3;
	for (uint_fast32_t y = 0; y < (y2 - y1); y++) {					// For each y line
		for (uint_fast32_t x = 0; x < (x2 - x1); x++) {				// For each x between x1 and x2
			video_wr_ptr[x] = Bc;									// Write the colour
		}
		video_wr_ptr += dc->wth;									// Offset to next line
	}
}

/*-[INTERNAL: VertLine16]---------------------------------------------------}
. 16 Bit colour version of the vertical line draw from (x,y) up or down cy
. pixels in the current text colour. The dc pointer is assumed valid as this
. is an internal call it is assumed to have been checked before call.
. 10Aug17 LdB
.--------------------------------------------------------------------------*/
static void VertLine16 (INTDC* dc, uint_fast32_t cy, int_fast8_t dir) {
	RGB565* __attribute__((aligned(2))) video_wr_ptr = (RGB565*)(uintptr_t)(dc->fb + (dc->curPos.y * dc->wth * 2) + (dc->curPos.x * 2));
	RGB565 Fc;
	Fc.R = dc->TxtColor.rgbRed >> 3;
	Fc.G = dc->TxtColor.rgbGreen >> 2;
	Fc.B = dc->TxtColor.rgbBlue >> 3;
	for (uint_fast32_t i = 0; i < cy; i++) {						// For each y line
		video_wr_ptr[0] = Fc;										// Write the colour
		if (dir == 1) video_wr_ptr += dc->wth;						// Positive offset to next line
			else  video_wr_ptr -= dc->wth;							// Negative offset to next line
	}
	dc->curPos.y += (cy * dir);										// Set current y position
}

/*-[INTERNAL: HorzLine16]---------------------------------------------------}
. 16 Bit colour version of the horizontal line draw from (x,y) left or right
. cx pixels in the current text colour. The dc pointer is assumed valid as
. this is an internal call it is assumed to have been checked before call.
. 10Aug17 LdB
.--------------------------------------------------------------------------*/
static void HorzLine16 (INTDC* dc, uint_fast32_t cx, int_fast8_t dir) {
	RGB565* __attribute__((aligned(2))) video_wr_ptr = (RGB565*)(uintptr_t)(dc->fb + (dc->curPos.y * dc->wth * 2) + (dc->curPos.x * 2));
	RGB565 Fc;
	Fc.R = dc->TxtColor.rgbRed >> 3;
	Fc.G = dc->TxtColor.rgbGreen >> 2;
	Fc.B = dc->TxtColor.rgbBlue >> 3;
	for (uint_fast32_t i = 0; i < cx; i++) {						// For each x pixel
		video_wr_ptr[0] = Fc;										// Write the colour
		video_wr_ptr += dir;										// Positive offset to next pixel
	}
	dc->curPos.x += (cx * dir);										// Set current x position
}

/*-[INTERNAL: DiagLine16]---------------------------------------------------}
. 16 Bit colour version of the diagonal line draw from (x,y) left or right
. cx pixels in the current text colour. The dc pointer is assumed valid as
. this is an internal call it is assumed to have been checked before call.
. 10Aug17 LdB
.--------------------------------------------------------------------------*/
static void DiagLine16 (INTDC* dc, uint_fast32_t dx, uint_fast32_t dy, int_fast8_t xdir, int_fast8_t ydir) {
	RGB565* __attribute__((aligned(2))) video_wr_ptr = (RGB565*)(uintptr_t)(dc->fb + (dc->curPos.y * dc->wth * 2) + (dc->curPos.x * 2));
	uint_fast32_t tx = 0;											// Zero test x value
	uint_fast32_t ty = 0;											// Zero test y value
	RGB565 Fc;
	Fc.R = dc->TxtColor.rgbRed >> 3;
	Fc.G = dc->TxtColor.rgbGreen >> 2;
	Fc.B = dc->TxtColor.rgbBlue >> 3;								// Colour for line
	uint_fast32_t eulerMax = dx;									// Start with dx value
	if (dy > eulerMax) dy = eulerMax;								// If dy greater change to that value
	for (uint_fast32_t i = 0; i < eulerMax; i++) {					// For euler steps
		video_wr_ptr[0] = Fc;										// Write pixel
		tx += dx;													// Increment test x value by dx
		if (tx >= eulerMax) {										// If tx >= eulerMax we step
			tx -= eulerMax;											// Subtract eulerMax
			video_wr_ptr += xdir;									// Move pointer left/right 1 pixel
		}
		ty += dy;													// Increment test y value by dy
		if (ty >= eulerMax) {										// If ty >= eulerMax we step
			ty -= eulerMax;											// Subtract eulerMax
			video_wr_ptr += (ydir*dc->wth);							// Move pointer up/down 1 line
		}
	}
	dc->curPos.x += (dx * xdir);									// Set current x2 position
	dc->curPos.y += (dy * ydir);									// Set current y2 position
}

//here
void SmartStartPutPixelRaw( printhandler prn_handler, uint32_t position, uint32_t color ){

  //prn_handler( " === INSIDE SMART PUT PIXEL RAW === " );
	hal_io_serial_puts( SerialA, "INSIDE SMART PUT PIXEL" );

	//RGB565* __attribute__((aligned(2))) video_wr_ptr = (RGB565*)(uintptr_t)(console.fb + position);
	//RGB565 Bc;
	//Bc.R = console.TxtColor.rgbRed >> 3;
	//Bc.G = console.TxtColor.rgbGreen >> 2;
	//Bc.B = console.TxtColor.rgbBlue >> 3;

	//*video_wr_ptr = Bc;								// Write pixel
}

/*-[INTERNAL: WriteChar16]--------------------------------------------------}
. 16 Bit colour version of the text character draw. The given character is
. drawn at the current position in the current text and background colours.
. 10Aug17 LdB
.--------------------------------------------------------------------------*/
static void WriteChar16 (INTDC* dc, uint8_t Ch) {
	RGB565* __attribute__((aligned(2))) video_wr_ptr = (RGB565*)(uintptr_t)(dc->fb + (dc->curPos.y * dc->wth * 2) + (dc->curPos.x * 2));
	RGB565 Fc, Bc;
	Fc.R = dc->TxtColor.rgbRed >> 3;
	Fc.G = dc->TxtColor.rgbGreen >> 2;
	Fc.B = dc->TxtColor.rgbBlue >> 3;								// Colour for text
	Bc.R = dc->BkColor.rgbRed >> 3;
	Bc.G = dc->BkColor.rgbGreen >> 2;
	Bc.B = dc->BkColor.rgbBlue >> 3;								// Colour for background
	for (uint_fast32_t y = 0; y < 4; y++) {
		uint32_t b = BitFont[(Ch * 4) + y];							// Fetch character bits
		for (uint_fast32_t i = 0; i < 32; i++) {					// For each bit
			RGB565 col = Bc;										// Preset background colour
			int xoffs = i % 8;										// X offset
			if ((b & 0x80000000) != 0) col = Fc;					// If bit set take text colour

			//HERE   !!!!!
			video_wr_ptr[xoffs] = col;								// Write pixel


			b <<= 1;												// Roll font bits left
			if (xoffs == 7) video_wr_ptr += dc->wth;				// If was bit 7 next line down
		}
	}
	dc->curPos.x += BitFontWth;										// Increment x position
}

/*-[INTERNAL: PutImage16]---------------------------------------------------}
. 16 Bit colour version of the put image draw. The image is transferred from
. the source position and drawn to screen. The source and destination format
. need to be the same and should be ensured by preprocess code.
. 10Aug17 LdB
.--------------------------------------------------------------------------*/
static void PutImage16 (INTDC* dc, uint_fast32_t dx, uint_fast32_t dy, IMAGE_PTR ImageSrc, bool BottomUp) {
	IMAGE_PTR video_wr_ptr;
	video_wr_ptr.ptrRGB565 = (RGB565*)(uintptr_t)(dc->fb + (dc->curPos.y * dc->wth * 2) + (dc->curPos.x * 2));
	for (uint_fast32_t y = 0; y < dy; y++) {						// For each line
		for (uint_fast32_t x = 0; x < dx; x++) {					// For each pixel
			video_wr_ptr.ptrRGB565[x] = *ImageSrc.ptrRGB565++;		// Transfer pixel
		}
		if (BottomUp) video_wr_ptr.ptrRGB565 -= dc->wth;			// Next line up
			else video_wr_ptr.ptrRGB565 += dc->wth;					// Next line down
	}
}

/*--------------------------------------------------------------------------}
{					   24 BIT COLOUR GRAPHICS ROUTINES						}
{--------------------------------------------------------------------------*/

/*-[INTERNAL: ClearArea24]--------------------------------------------------}
. 24 Bit colour version of the clear area call which block fills the given
. area from (x1,y1) to (x2,y2) with the current brush colour. As an internal
. function the pairs are assumed to be correvtly ordered and dc valid.
. 10Aug17 LdB
.--------------------------------------------------------------------------*/
static void ClearArea24 (INTDC* dc, uint_fast32_t x1, uint_fast32_t y1, uint_fast32_t x2, uint_fast32_t y2) {
	RGB* __attribute__((aligned(1))) video_wr_ptr = (RGB*)(uintptr_t)(dc->fb + (y1 * dc->wth * 3) + (x1 * 3));
	for (uint_fast32_t y = 0; y < (y2 - y1); y++) {					// For each y line
		for (uint_fast32_t x = 0; x < (x2 - x1); x++) {				// For each x between x1 and x2
			video_wr_ptr[x] = dc->BrushColor.rgb;					// Write the colour
		}
		video_wr_ptr += dc->wth;									// Offset to next line
	}
}

/*-[INTERNAL: VertLine24]---------------------------------------------------}
. 24 Bit colour version of the vertical line draw from (x,y) up or down cy
. pixels in the current text colour. The dc pointer is assumed valid as this
. is an internal call it is assumed to have been checked before call.
. 10Aug17 LdB
.--------------------------------------------------------------------------*/
static void VertLine24 (INTDC* dc, uint_fast32_t cy, int_fast8_t dir) {
	RGB* __attribute__((aligned(1))) video_wr_ptr = (RGB*)(uintptr_t)(dc->fb + (dc->curPos.y * dc->wth * 3) + (dc->curPos.x * 3));
	for (uint_fast32_t i = 0; i < cy; i++) {						// For each y line
		video_wr_ptr[0] = dc->TxtColor.rgb;							// Write the colour
		if (dir == 1) video_wr_ptr += dc->wth;						// Positive offset to next line
			else  video_wr_ptr -= dc->wth;							// Negative offset to next line
	}
	dc->curPos.y += (cy * dir);										// Set current y position
}

/*-[INTERNAL: HorzLine24]---------------------------------------------------}
. 24 Bit colour version of the horizontal line draw from (x,y) left or right
. cx pixels in the current text colour. The dc pointer is assumed valid as
. this is an internal call it is assumed to have been checked before call.
. 10Aug17 LdB
.--------------------------------------------------------------------------*/
static void HorzLine24 (INTDC* dc, uint_fast32_t cx, int_fast8_t dir) {
	RGB* __attribute__((aligned(1))) video_wr_ptr = (RGB*)(uintptr_t)(dc->fb + (dc->curPos.y * dc->wth * 3) + (dc->curPos.x * 3));
	for (uint_fast32_t i = 0; i < cx; i++) {						// For each x pixel
		video_wr_ptr[0] = dc->TxtColor.rgb;							// Write the colour
		video_wr_ptr += dir;										// Positive offset to next pixel
	}
	dc->curPos.x += (cx * dir);										// Set current x position
}

/*-[INTERNAL: DiagLine24]---------------------------------------------------}
. 24 Bit colour version of the diagonal line draw from (x,y) left or right
. cx pixels in the current text colour. The dc pointer is assumed valid as
. this is an internal call it is assumed to have been checked before call.
. 10Aug17 LdB
.--------------------------------------------------------------------------*/
static void DiagLine24 (INTDC* dc, uint_fast32_t dx, uint_fast32_t dy, int_fast8_t xdir, int_fast8_t ydir) {
	RGB* __attribute__((aligned(1))) video_wr_ptr = (RGB*)(uintptr_t)(dc->fb + (dc->curPos.y * dc->wth * 3) + (dc->curPos.x * 3));
	uint_fast32_t tx = 0;
	uint_fast32_t ty = 0;
	uint_fast32_t eulerMax = dx;									// Start with dx value
	if (dy > eulerMax) dy = eulerMax;								// If dy greater change to that value
	for (uint_fast32_t i = 0; i < eulerMax; i++) {					// For euler steps
		video_wr_ptr[0] = dc->TxtColor.rgb;							// Write pixel
		tx += dx;													// Increment test x value by dx
		if (tx >= eulerMax) {										// If tx >= eulerMax we step
			tx -= eulerMax;											// Subtract eulerMax
			video_wr_ptr += xdir;									// Move pointer left/right 1 pixel
		}
		ty += dy;													// Increment test y value by dy
		if (ty >= eulerMax) {										// If ty >= eulerMax we step
			ty -= eulerMax;											// Subtract eulerMax
			video_wr_ptr += (ydir*dc->wth);							// Move pointer up/down 1 line
		}
	}
	dc->curPos.x += (dx * xdir);									// Set current x2 position
	dc->curPos.y += (dy * ydir);									// Set current y2 position
}

/*-[INTERNAL: WriteChar24]--------------------------------------------------}
. 24 Bit colour version of the text character draw. The given character is
. drawn at the current position in the current text and background colours.
. 10Aug17 LdB
.--------------------------------------------------------------------------*/
static void WriteChar24 (INTDC* dc, uint8_t Ch) {
	RGB* __attribute__((aligned(1))) video_wr_ptr = (RGB*)(uintptr_t)(dc->fb + (dc->curPos.y * dc->wth * 3) + (dc->curPos.x * 3));
	for (uint_fast32_t y = 0; y < 4; y++) {
		uint32_t b = BitFont[(Ch * 4) + y];							// Fetch character bits
		for (uint_fast32_t i = 0; i < 32; i++) {					// For each bit
			RGB col = dc->BkColor.rgb;								// Preset background colour
			int xoffs = i % 8;										// X offset
			if ((b & 0x80000000) != 0) col = dc->TxtColor.rgb;		// If bit set take text colour
			video_wr_ptr[xoffs] = col;								// Write pixel
			b <<= 1;												// Roll font bits left
			if (xoffs == 7) video_wr_ptr += dc->wth;				// If was bit 7 next line down
		}
	}
	dc->curPos.x += BitFontWth;										// Increment x position
}

/*-[INTERNAL: PutImage24]---------------------------------------------------}
. 24 Bit colour version of the put image draw. The image is transferred from
. the source position and drawn to screen. The source and destination format
. need to be the same and should be ensured by preprocess code.
. 10Aug17 LdB
.--------------------------------------------------------------------------*/
static void PutImage24(INTDC* dc, uint_fast32_t dx, uint_fast32_t dy, IMAGE_PTR ImageSrc, bool BottomUp) {
	IMAGE_PTR video_wr_ptr;
	video_wr_ptr.ptrRGB = (RGB*)(uintptr_t)(dc->fb + (dc->curPos.y * dc->wth * 3) + (dc->curPos.x * 3));
	for (uint_fast32_t y = 0; y < dy; y++) {						// For each line
		for (uint_fast32_t x = 0; x < dx; x++) {					// For each pixel
			video_wr_ptr.ptrRGB[x] = *ImageSrc.ptrRGB++;			// Transfer pixel
		}
		if (BottomUp) video_wr_ptr.ptrRGB -= dc->wth;				// Next line up
			else video_wr_ptr.ptrRGB += dc->wth;					// Next line down
	}
}

/*--------------------------------------------------------------------------}
{					   32 BIT COLOUR GRAPHICS ROUTINES						}
{--------------------------------------------------------------------------*/

/*-[INTERNAL: ClearArea32]--------------------------------------------------}
. 32 Bit colour version of the clear area call which block fills the given
. area from (x1,y1) to (x2,y2) with the current brush colour. As an internal
. function the (x1,y1) pairs are assumed to be smaller than (x2,y2).
. 10Aug17 LdB
.--------------------------------------------------------------------------*/
static void ClearArea32 (INTDC* dc, uint_fast32_t x1, uint_fast32_t y1, uint_fast32_t x2, uint_fast32_t y2) {
	RGBA* __attribute__((aligned(4))) video_wr_ptr = (RGBA*)(uintptr_t)(dc->fb + (y1 * dc->wth * 4) + (y1 * 4));
	for (uint_fast32_t y = 0; y < (y2 - y1); y++) {					// For each y line
		for (uint_fast32_t x = 0; x < (x2 - x1); x++) {				// For each x between x1 and x2
			video_wr_ptr[x] = dc->BrushColor;						// Write the current brush colour
		}
		video_wr_ptr += dc->wth;									// Next line down
	}
}

/*-[INTERNAL: VertLine32]---------------------------------------------------}
. 32 Bit colour version of the vertical line draw from (x,y) up or down cy
. pixels in the current text colour. The dc pointer is assumed valid as this
. is an internal call it is assumed to have been checked before call.
. 10Aug17 LdB
.--------------------------------------------------------------------------*/
static void VertLine32 (INTDC* dc, uint_fast32_t cy, int_fast8_t dir) {
	RGBA* __attribute__((aligned(4))) video_wr_ptr = (RGBA*)(uintptr_t)(dc->fb + (dc->curPos.y * dc->wth * 4) + (dc->curPos.x * 4));
	for (uint_fast32_t i = 0; i < cy; i++) {						// For each y line
		video_wr_ptr[0] = dc->TxtColor;								// Write the colour
		if (dir == 1) video_wr_ptr += dc->wth;						// Positive offset to next line
			else  video_wr_ptr -= dc->wth;							// Negative offset to next line
	}
	dc->curPos.y += (cy * dir);										// Set current y position
}

/*-[INTERNAL: HorzLine32]---------------------------------------------------}
. 32 Bit colour version of the horizontal line draw from (x,y) left or right
. cx pixels in the current text colour. The dc pointer is assumed valid as
. this is an internal call it is assumed to have been checked before call.
. 10Aug17 LdB
.--------------------------------------------------------------------------*/
static void HorzLine32 (INTDC* dc, uint_fast32_t cx, int_fast8_t dir) {
	RGBA* __attribute__((aligned(4))) video_wr_ptr = (RGBA*)(uintptr_t)(dc->fb + (dc->curPos.y * dc->wth * 4) + (dc->curPos.x * 4));
	for (uint_fast32_t i = 0; i < cx; i++) {						// For each x pixel
		video_wr_ptr[0] = dc->TxtColor;								// Write the colour
		video_wr_ptr += dir;										// Positive offset to next pixel
	}
	dc->curPos.x += (cx * dir);										// Set current x position
}

/*-[INTERNAL: DiagLine32]---------------------------------------------------}
. 32 Bit colour version of the diagonal line draw from (x,y) left or right
. cx pixels in the current text colour. The dc pointer is assumed valid as
. this is an internal call it is assumed to have been checked before call.
. 10Aug17 LdB
.--------------------------------------------------------------------------*/
static void DiagLine32 (INTDC* dc, uint_fast32_t dx, uint_fast32_t dy, int_fast8_t xdir, int_fast8_t ydir) {
	RGBA* __attribute__((aligned(4))) video_wr_ptr = (RGBA*)(uintptr_t)(dc->fb + (dc->curPos.y * dc->wth * 4) + (dc->curPos.x * 4));
	uint_fast32_t tx = 0;
	uint_fast32_t ty = 0;
	uint_fast32_t eulerMax = dx;									// Start with dx value
	if (dy > eulerMax) dy = eulerMax;								// If dy greater change to that value
	for (uint_fast32_t i = 0; i < eulerMax; i++) {					// For euler steps
		video_wr_ptr[0] = dc->TxtColor;								// Write pixel
		tx += dx;													// Increment test x value by dx
		if (tx >= eulerMax) {										// If tx >= eulerMax we step
			tx -= eulerMax;											// Subtract eulerMax
			video_wr_ptr += xdir;									// Move pointer left/right 1 pixel
		}
		ty += dy;													// Increment test y value by dy
		if (ty >= eulerMax) {										// If ty >= eulerMax we step
			ty -= eulerMax;											// Subtract eulerMax
			video_wr_ptr += (ydir*dc->wth);							// Move pointer up/down 1 line
		}
	}
	dc->curPos.x += (dx * xdir);									// Set current x2 position
	dc->curPos.y += (dy * ydir);									// Set current y2 position
}

/*-[INTERNAL: WriteChar32]--------------------------------------------------}
. 32 Bit colour version of the text character draw. The given character is
. drawn at the current position in the current text and background colours.
. 10Aug17 LdB
.--------------------------------------------------------------------------*/
static void WriteChar32 (INTDC* dc, uint8_t Ch) {
	RGBA* __attribute__((__packed__, aligned(4))) video_wr_ptr = (RGBA*)(uintptr_t)(dc->fb + (dc->curPos.y * dc->wth * 4) + (dc->curPos.x * 4));
	for (uint_fast32_t y = 0; y < 4; y++) {
		uint32_t b = BitFont[(Ch * 4) + y];							// Fetch character bits
		for (uint_fast32_t i = 0; i < 32; i++) {					// For each bit
			RGBA col = dc->BkColor;									// Preset background colour
			uint_fast8_t xoffs = i % 8;								// X offset
			if ((b & 0x80000000) != 0) col = dc->TxtColor;			// If bit set take text colour
			//video_wr_ptr[xoffs] = col;								// Write pixel
			b <<= 1;												// Roll font bits left
			if (xoffs == 7) video_wr_ptr += dc->wth;				// If was bit 7 next line down
		}
	}
	dc->curPos.x += BitFontWth;										// Increment x position
}

/*-[INTERNAL: PutImage32]---------------------------------------------------}
. 32 Bit colour version of the put image draw. The image is transferred from
. the source position and drawn to screen. The source and destination format
. need to be the same and should be ensured by preprocess code.
. 10Aug17 LdB
.--------------------------------------------------------------------------*/
static void PutImage32 (INTDC* dc, uint_fast32_t dx, uint_fast32_t dy, IMAGE_PTR ImageSrc, bool BottomUp) {
	IMAGE_PTR video_wr_ptr;
	video_wr_ptr.ptrRGBA = (RGBA*)(uintptr_t)(dc->fb + (dc->curPos.y * dc->wth * 4) + (dc->curPos.x * 4));
	for (uint_fast32_t y = 0; y < dy; y++) {						// For each line
		for (uint_fast32_t x = 0; x < dx; x++) {					// For each pixel
			video_wr_ptr.ptrRGBA[x] = *ImageSrc.ptrRGBA++;			// Transfer pixel
		}
		if (BottomUp) video_wr_ptr.ptrRGBA -= dc->wth;				// Next line up
			else video_wr_ptr.ptrRGBA += dc->wth;					// Next line down
	}
}



BOOL Rectangle(HDC hdc,
	int_fast32_t nLeftRect,
	int_fast32_t nTopRect,
	int_fast32_t nRightRect,
	int_fast32_t nBottomRect)
{
	INTDC* intDC = (INTDC*)hdc;										// Typecast hdc to internal DC
	if (intDC) {
		intDC->ClearArea(intDC, nLeftRect, nTopRect, nRightRect, nBottomRect);
		return TRUE;
	}
	return FALSE;
}


bool PiConsole_Init (int Width, int Height, int Depth, printhandler prn_handler) {
	uint32_t buffer[19];
	if ((Width == 0) || (Height == 0)) {							// Has auto width or heigth been requested
		if (mailbox_tag_message(&buffer[0], 5,
			MAILBOX_TAG_GET_PHYSICAL_WIDTH_HEIGHT,
			8, 0, 0, 0)) {											// Get current width and height of screen
			if (Width == 0) Width = buffer[3];						// Width passed in as zero set set current screen width
			if (Height == 0) Height = buffer[4];					// Height passed in as zero set set current screen height
		} else return false;										// For some reason get screen physical failed
	}
	if (Depth == 0) {												// Has auto colour depth been requested
		if (mailbox_tag_message(&buffer[0], 4,
			MAILBOX_TAG_GET_COLOUR_DEPTH,
			4, 4, 0)) {												// Get current colour depth of screen
			Depth = buffer[3];										// Depth passed in as zero set set current screen colour depth
		} else return false;										// For some reason get screen depth failed
	}
	if (!mailbox_tag_message(&buffer[0], 19,
		MAILBOX_TAG_SET_PHYSICAL_WIDTH_HEIGHT, 8, 8, Width, Height,
		MAILBOX_TAG_SET_VIRTUAL_WIDTH_HEIGHT, 8, 8, Width, Height,
		MAILBOX_TAG_SET_COLOUR_DEPTH, 4, 4, Depth,
		MAILBOX_TAG_ALLOCATE_FRAMEBUFFER, 8, 4, 16, 0)) return false;
	console.fb = GPUaddrToARMaddr(buffer[17]);

	console.TxtColor.ref = 0xFFFFFFFF;
	console.BkColor.ref = 0x00000000;
	console.BrushColor.ref = 0xFF00FF00;
	console.wth = Width;
	console.ht = Height;
	console.depth = Depth;

	switch (Depth) {
	case 32:														/* 32 bit colour screen mode */
		console.ClearArea = ClearArea32;							// Set console function ptr to 32bit colour version of clear area
		console.VertLine = VertLine32;								// Set console function ptr to 32bit colour version of vertical line
		console.HorzLine = HorzLine32;								// Set console function ptr to 32bit colour version of horizontal line
		console.DiagLine = DiagLine32;								// Set console function ptr to 32bit colour version of diagonal line
		console.WriteChar = WriteChar32;							// Set console function ptr to 32bit colour version of write character
		console.PutImage = PutImage32;								// Set console function ptr to 32bit colour version of put bitmap image
		break;
	case 24:														/* 24 bit colour screen mode */
		console.ClearArea = ClearArea24;							// Set console function ptr to 24bit colour version of clear area
		console.VertLine = VertLine24;								// Set console function ptr to 24bit colour version of vertical line
		console.HorzLine = HorzLine24;								// Set console function ptr to 24bit colour version of horizontal line
		console.DiagLine = DiagLine24;								// Set console function ptr to 24bit colour version of diagonal line
		console.WriteChar = WriteChar24;							// Set console function ptr to 24bit colour version of write character
		console.PutImage = PutImage24;								// Set console function ptr to 24bit colour version of put bitmap image
		break;
	case 16:														/* 16 bit colour screen mode */
		console.ClearArea = ClearArea16;							// Set console function ptr to 16bit colour version of clear area
		console.VertLine = VertLine16;								// Set console function ptr to 16bit colour version of vertical line
		console.HorzLine = HorzLine16;								// Set console function ptr to 16bit colour version of horizontal line
		console.DiagLine = DiagLine16;								// Set console function ptr to 16bit colour version of diagonal line
		console.WriteChar = WriteChar16;							// Set console function ptr to 16bit colour version of write character
		console.PutImage = PutImage16;								// Set console function ptr to 16bit colour version of put bitmap image
		break;
	}

	if (prn_handler) prn_handler("Screen resolution %i x %i Colour Depth: %i\n",
		Width, Height, Depth);										// If print handler valid print the display resolution message
	return true;
}

HDC GetConsoleDC(void) {
	return (HDC)&console;
}

uint32_t GetConsole_FrameBuffer(void) {
	return (uint32_t)console.fb;
}

uint32_t GetConsole_Width(void) {
	return (uint32_t)console.wth;
}

uint32_t GetConsole_Height(void) {
	return (uint32_t)console.ht;
}


/* Increase program data space. As malloc and related functions depend on this,
it is useful to have a working implementation. The following suffices for a
standalone system; it exploits the symbol _end automatically defined by the
GNU linker. */
#include <sys/types.h>
caddr_t __attribute__((weak)) _sbrk (int incr)
{
	extern char _end;
	static char* heap_end = 0;
	char* prev_heap_end;

	if (heap_end == 0)
		heap_end = &_end;

	prev_heap_end = heap_end;
	heap_end += incr;

	return (caddr_t)prev_heap_end;
}

#include <errno.h>
#undef errno
extern int errno;
/* Send a signal. Minimal implementation: */
int __attribute__((weak)) _kill (int pid, int sig)
{
	errno = EINVAL;
	return -1;
}

/* Process-ID; this is sometimes used to generate strings unlikely to conflict
with other processes. Minimal implementation, for a system without
processes: */
int __attribute__((weak)) _getpid(void)
{
	return 1;
}


/* Read from a file. Minimal implementation: */
int __attribute__((weak)) _read(int file, char *ptr, int len)
{
	return 0;
}

/* There's currently no implementation of a file system because there's no
file system! */
int __attribute__((weak)) _close(int file)
{
	return -1;
}

/* Set position in a file. Minimal implementation: */
int __attribute__((weak)) _lseek(int file, int ptr, int dir)
{
	return 0;
}

/* Query whether output stream is a terminal. For consistency with the other
minimal implementations, which only support output to stdout, this minimal
implementation is suggested: */
int __attribute__((weak)) _isatty(int file)
{
	return 1;
}

/* Status of an open file. For consistency with other minimal implementations
in these examples, all files are regarded as character special devices. The
sys/stat.h header file required is distributed in the include subdirectory
for this C library. */
#include <sys/stat.h>
int __attribute__((weak)) _fstat(int file, struct stat *st)
{
	st->st_mode = S_IFCHR;
	return 0;
}

/* Never return from _exit as there's no OS to exit to, so instead we trap here */
void __attribute__((weak)) _exit(int status)
{
	/* Stop the compiler complaining about unused variables by "using" it */
	(void)status;

	while (1)
	{
		/* TRAP HERE */
	}
}

int _write (int file, char *ptr, int len) {

	for (int todo = 0; todo < len; todo++) {
		char Ch = *ptr++;
		Embedded_Console_WriteChar(Ch);
	}
	return len;
}
