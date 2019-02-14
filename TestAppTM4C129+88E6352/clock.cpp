
#include "clock.h"
#include <TM4C1294KCPDT.h>

static volatile uint32_t tick_count;
static volatile uint32_t frequency;
static volatile uint32_t muClockCyclesPer_uSec;

static constexpr uint32_t SYSTICKHZ = 100;

// Calculate the system frequency from the register settings base on the oscillator input.
/*
static uint32_t _SysCtlFrequencyGet (uint32_t ui32Xtal)
{
    uint32_t ui32Result;
    uint_fast16_t ui16F1, ui16F2;
    uint_fast16_t ui16PInt, ui16PFract;
    uint_fast8_t ui8Q, ui8N;

    //
    // Extract all of the values from the hardware registers.
    //
    ui16PFract = ((HWREG(SYSCTL_PLLFREQ0) & SYSCTL_PLLFREQ0_MFRAC_M) >>
                  SYSCTL_PLLFREQ0_MFRAC_S);
    ui16PInt = HWREG(SYSCTL_PLLFREQ0) & SYSCTL_PLLFREQ0_MINT_M;
    ui8Q = (((HWREG(SYSCTL_PLLFREQ1) & SYSCTL_PLLFREQ1_Q_M) >>
             SYSCTL_PLLFREQ1_Q_S) + 1);
    ui8N = (((HWREG(SYSCTL_PLLFREQ1) & SYSCTL_PLLFREQ1_N_M) >>
             SYSCTL_PLLFREQ1_N_S) + 1);

    //
    // Divide the crystal value by N.
    //
    ui32Xtal /= (uint32_t)ui8N;

    //
    // Calculate the multiplier for bits 9:5.
    //
    ui16F1 = ui16PFract / 32;

    //
    // Calculate the multiplier for bits 4:0.
    //
    ui16F2 = ui16PFract - (ui16F1 * 32);

    //
    // Get the integer portion.
    //
    ui32Result = ui32Xtal * (uint32_t)ui16PInt;

    //
    // Add first fractional bits portion(9:0).
    //
    ui32Result += (ui32Xtal * (uint32_t)ui16F1) / 32;

    //
    // Add the second fractional bits portion(4:0).
    //
    ui32Result += (ui32Xtal * (uint32_t)ui16F2) / 1024;

    //
    // Divide the result by Q.
    //
    ui32Result = ui32Result / (uint32_t)ui8Q;

    //
    // Return the resulting PLL frequency.
    //
    return(ui32Result);
}
*/

// Run from the PLL at 120 MHz.
void clock_init ()
{

	SYSCTL->MOSCCTL = (1 << 4) // OSCRNG = 1 (freq higher than 10 MHz)
		| (0 << 3) // PWRDN
		| (0 << 2) // NOXTAL
		| (0 << 1) // MOSCIM
		| (0 << 0); // CVAL

	// Wait for the hardware to set MOSCPUPRIS (MOSC reached expected frequency)
	while ((SYSCTL->RIS & (1 << 8)) == 0)
		;

	SYSCTL->PLLFREQ1 = (1 << 8) | (4 << 0); // Q=1, N=4
	SYSCTL->PLLFREQ0 = (1 << 23) // PLLPWR
		| (0 << 10)  // MFRAC
		| (96 << 0); // MINT

	// Set the Flash and EEPROM timing values.
	SYSCTL->MEMTIM0 = (6 << 22) // EBCHT
		| (0 << 21) // EBCE
		| (5 << 16) // EWS
		| (6 << 6)  // FBCHT
		| (0 << 5)  // FBCE
		| (5 << 0); // FWS

	// Trigger the PLL to lock to the new frequency by setting the NEWFREQ bit.
	SYSCTL->RSCLKCFG |= (1 << 30);

	// Wait until the PLL has locked (until the hardware sets the LOCK bit)
	while ((SYSCTL->PLLSTAT & 1) == 0)
		;

	SYSCTL->RSCLKCFG = (0u << 31) // MEMTIMU
		| (0 << 30) // NEWFREQ
		| (0 << 29) // ACG
		| (1 << 28) // USEPLL
		| (3 << 24) // PLLSRC
		| (3 << 20) // OSCSRC
		| (0 << 10) // OSYSDIV
		| (1 << 0); // PSYSDIV

    // Finally change the OSCSRC back to PIOSC
    SYSCTL->RSCLKCFG &= ~(0xF << 20);

	//uint32_t frequency = _SysCtlFrequencyGet(ui32Osc) / 2;
	frequency = 120000000;

	tick_count = 0;

	SysTick->LOAD = (frequency / SYSTICKHZ) - 1;

	SysTick->CTRL = (1 << 2) // CLK_SRC = 1 (clock source is system clock)
		| (1 << 1) // INTEN
		| (1 << 0); // ENABLE
}

uint32_t clock_get_freq()
{
	return frequency;
}

uint32_t clock_get_time_ms()
{
	return tick_count * (1000 / SYSTICKHZ);
}

extern "C" void SysTick_Handler()
{
	tick_count++;
}
