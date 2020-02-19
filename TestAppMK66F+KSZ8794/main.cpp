
#include "drivers/clock.h"
#include "drivers/gpio.h"
#include "drivers/pit.h"
#include "drivers/ethernet.h"
#include "switch.h"
#include <CMSIS/MK66F18.h>
#include <debugio.h>
#include <assert.h>

extern "C" void __assert(const char *__expression, const char *__filename, int __line)
{
	debug_printf("Assertion failure.\n");
	__BKPT();
}

#undef SYSTEM_MCG_C5_VALUE
#undef SYSTEM_MCG_C6_VALUE
#undef SYSTEM_MCG_C9_VALUE
#undef SYSTEM_SIM_CLKDIV2_VALUE

/* MCG_C1: CLKS=0,FRDIV=4,IREFS=0,IRCLKEN=1,IREFSTEN=0 */
#define SYSTEM_MCG_C1_VALUE 0x22U /* MCG_C1 */
/* MCG_C2: LOCRE0=0,FCFTRIM=0,RANGE=2,HGO=0,EREFS=1,LP=0,IRCS=0 */
#define SYSTEM_MCG_C2_VALUE 0x24U /* MCG_C2 */
/* MCG_C4: DMX32=0,DRST_DRS=0,FCTRIM=0,SCFTRIM=0 */
#define SYSTEM_MCG_C4_VALUE 0x00U /* MCG_C4 */
/* MCG_SC: ATME=0,ATMS=0,ATMF=0,FLTPRSRV=0,FCRDIV=0,LOCS0=0 */
#define SYSTEM_MCG_SC_VALUE 0x00U /* MCG_SC */
/* MCG_C5: PLLCLKEN=0,PLLSTEN=0,PRDIV=0 */
#define SYSTEM_MCG_C5_VALUE 0x00U /* MCG_C5 */
/* MCG_C6: LOLIE0=0,PLLS=1,CME0=0,VDIV=0x0C */
#define SYSTEM_MCG_C6_VALUE 0x4CU /* MCG_C6 */
/* MCG_C7: OSCSEL=0 */
#define SYSTEM_MCG_C7_VALUE 0x00U /* MCG_C7 */
/* MCG_C9: PLL_CME=0,PLL_LOCRE=0,EXT_PLL_LOCS=0 */
#define SYSTEM_MCG_C9_VALUE 0x00U /* MCG_C9 */
/* MCG_C11: PLLCS=0 */
#define SYSTEM_MCG_C11_VALUE 0x00U /* MCG_C11 */
/* OSC_CR: ERCLKEN=1,EREFSTEN=0,SC2P=0,SC4P=0,SC8P=0,SC16P=0 */
#define SYSTEM_OSC_CR_VALUE 0x80U /* OSC_CR */
/* SIM_CLKDIV2: USBDIV=6,USBFRAC=1 */
#define SYSTEM_SIM_CLKDIV2_VALUE 0x0DU /* SIM_CLKDIV2 */
/* SIM_CLKDIV3: PLLFLLDIV=0,PLLFLLFRAC=0 */
#define SYSTEM_SIM_CLKDIV3_VALUE 0x00U /* SIM_CLKDIV3 */
/* SIM_SOPT1: USBREGEN=0,USBSSTBY=0,USBVSTBY=0,OSC32KSEL=3,RAMSIZE=0 */

extern "C" void SystemInit()
{
	// Disable the watchdog
	WDOG->UNLOCK = 0xC520;
	WDOG->UNLOCK = 0xD928;
	WDOG->STCTRLH = 0x1D2;

	//MPU_CESR = 0; // Disable the MPU. All accesses from all bus masters are allowed.

	SMC->PMPROT = 0xAA; // allow entering all power modes

    // Enable HSRUN mode
    SMC->PMCTRL = 0x60;
    while (SMC->PMSTAT != 0x80)
        ;

    SIM->CLKDIV1 = (0 << 28)  // OUTDIV1=0, divide MCGOUTCLK by 1 to get the core/system clock
	             | (7 << 24)  // OUTDIV2=7, divide MCGOUTCLK by 8 to get the bus clock
	             | (2 << 20)  // OUTDIV3=
	             | (5 << 16); // OUTDIV4=
    SIM->SOPT2 = SIM->SOPT2 & ~SIM_SOPT2_PLLFLLSEL_MASK | (1 << SIM_SOPT2_PLLFLLSEL_SHIFT); // PLL/FLL clock select: MCGPLLCLK clock

    SIM_SCGC5 |= SIM_SCGC5_PORTA_MASK;
    /* PORTA_PCR18: ISF=0,MUX=0 */
    PORTA_PCR18 &= (uint32_t) ~(uint32_t)((PORT_PCR_ISF_MASK | PORT_PCR_MUX(0x07)));
    if (((SYSTEM_MCG_C2_VALUE)&MCG_C2_EREFS_MASK) != 0x00U)
    {
        /* PORTA_PCR19: ISF=0,MUX=0 */
        PORTA_PCR19 &= (uint32_t) ~(uint32_t)((PORT_PCR_ISF_MASK | PORT_PCR_MUX(0x07)));
    }
    MCG->SC = SYSTEM_MCG_SC_VALUE;                                                                                  /* Set SC (fast clock internal reference divider) */
    MCG->C2 = (MCG->C2 & (uint8_t)(~(MCG_C2_FCFTRIM_MASK))) | (SYSTEM_MCG_C2_VALUE & (uint8_t)(~(MCG_C2_LP_MASK))); /* Set C2 (freq. range, ext. and int. reference selection etc. excluding trim bits; low power bit is set later) */
    OSC->CR = SYSTEM_OSC_CR_VALUE;                                                                                  /* Set OSC_CR (OSCERCLK enable, oscillator capacitor load) */
    MCG->C7 = SYSTEM_MCG_C7_VALUE;                                                                                  /* Set C7 (OSC Clock Select) */

    MCG->C1 = (SYSTEM_MCG_C1_VALUE) | MCG_C1_CLKS(0x02); /* Set C1 (clock source selection, FLL ext. reference divider, int. reference enable etc.) - PBE mode*/

    if ((((SYSTEM_MCG_C2_VALUE)&MCG_C2_EREFS_MASK) != 0x00U) && (((SYSTEM_MCG_C7_VALUE)&MCG_C7_OSCSEL_MASK) == 0x00U))
    {
        while ((MCG->S & MCG_S_OSCINIT0_MASK) == 0x00U)
        { /* Check that the oscillator is running */
        }
    }
    /* Check that the source of the FLL reference clock is the requested one. */
    if (((SYSTEM_MCG_C1_VALUE)&MCG_C1_IREFS_MASK) != 0x00U)
    {
        while ((MCG->S & MCG_S_IREFST_MASK) == 0x00U)
        {
        }
    }
    else
    {
        while ((MCG->S & MCG_S_IREFST_MASK) != 0x00U)
        {
        }
    }
    MCG->C4 = ((SYSTEM_MCG_C4_VALUE) & (uint8_t)(~(MCG_C4_FCTRIM_MASK | MCG_C4_SCFTRIM_MASK))) | (MCG->C4 & (MCG_C4_FCTRIM_MASK | MCG_C4_SCFTRIM_MASK)); /* Set C4 (FLL output; trim values not changed) */

    /* PLL clock can be used to generate clock for some devices regardless of clock generator (MCGOUTCLK) mode. */
    MCG->C5 = (SYSTEM_MCG_C5_VALUE) & (uint8_t)(~(MCG_C5_PLLCLKEN_MASK)); /* Set C5 (PLL settings, PLL reference divider etc.) */
    MCG->C6 = (SYSTEM_MCG_C6_VALUE) & (uint8_t) ~(MCG_C6_PLLS_MASK);      /* Set C6 (PLL select, VCO divider etc.) */
    if ((SYSTEM_MCG_C5_VALUE)&MCG_C5_PLLCLKEN_MASK)
    {
        MCG->C5 |= MCG_C5_PLLCLKEN_MASK; /* PLL clock enable in mode other than PEE or PBE */
    }

    MCG_C11 = SYSTEM_MCG_C11_VALUE; /* Set C11 (Select PLL used to derive MCGOUT */
    MCG->C6 |= (MCG_C6_PLLS_MASK);  /* Set C6 (PLL select, VCO divider etc.) */

	// Wait until PLL is locked
    while ((MCG->S & MCG_S_LOCK0_MASK) == 0x00U)
		;

    MCG->C1 &= (uint8_t) ~(MCG_C1_CLKS_MASK);

	// Wait until output of the PLL is selected.
    while ((MCG->S & MCG_S_CLKST_MASK) != 0x0CU)
		;

	// Wait until output of the correct PLL is selected
	while (MCG->S2 != SYSTEM_MCG_C11_VALUE)
		;

    SIM->CLKDIV2 = ((SIM->CLKDIV2) & (uint32_t)(~(SIM_CLKDIV2_USBFRAC_MASK | SIM_CLKDIV2_USBDIV_MASK))) | ((SYSTEM_SIM_CLKDIV2_VALUE) & (SIM_CLKDIV2_USBFRAC_MASK | SIM_CLKDIV2_USBDIV_MASK)); /* Selects the USB clock divider. */
    SIM->CLKDIV3 = ((SIM->CLKDIV3) & (uint32_t)(~(SIM_CLKDIV3_PLLFLLFRAC_MASK | SIM_CLKDIV3_PLLFLLDIV_MASK))) | ((SYSTEM_SIM_CLKDIV3_VALUE) & (SIM_CLKDIV3_PLLFLLFRAC_MASK | SIM_CLKDIV3_PLLFLLDIV_MASK)); /* Selects the PLLFLL clock divider. */
}

static const ethernet_pins eth_pins =
{
	.rmii_ref_clk = { { PTE, 26 }, 2 },
	.rmii_crs_dv  = { { PTA, 14 }, 4 },
	.rmii_rxd0    = { { PTA, 13 }, 4 },
	.rmii_rxd1    = { { PTA, 12 }, 4 },
	.rmii_tx_en   = { { PTA, 15 }, 4 },
	.rmii_txd0    = { { PTA, 16 }, 4 },
	.rmii_txd1    = { { PTA, 17 }, 4 },
	.rmii_mdio    = { },
	.rmii_mdc     = { },
};

static const uint8_t default_mac_address[] = { 0x10, 0x20, 0x30, 0x40, 0x50, 0xA0 };

static uint32_t time_ms;

static void pit_callback()
{
	time_ms++;
	if ((time_ms % 1000) == 0)
		gpio_toggle (PTA, 9);
}

int main()
{
    //	clock_init (12);

	MPU_CESR = 0; // Disable MPU. All accesses from all bus masters are allowed. The Ethernet peripheral needs this.

	pit_init(0, 21000, pit_callback);

	// Reset the switch
	gpio_make_output(PTA, 24, false);
	auto start_time = time_ms;
	while (time_ms - start_time < 3)
		;
	gpio_set (PTA, 24, true);

	// initialize the led pins (leds off)
	gpio_make_output (PTA, 8, true);
	gpio_make_output (PTA, 9, true);
	gpio_make_output (PTA, 10, true);
	gpio_make_output (PTA, 11, true);

	switch_init();

	SIM->SOPT2 |= SIM_SOPT2_RMIISRC_MASK; // Tell the Ethernet peripheral to take its clock from ENET_1588_CLKIN (not EXTAL)
	gpio_make_alternate(PTE, 26, 2);
	PORTE_PCR26 = PORT_PCR_MUX(0x02) |  PORT_PCR_ODE_MASK; // Set PTE26 as ENET_1588_CLKIN
	// TODO: read mac address from Flash
	enet_init (eth_pins, default_mac_address);

	volatile auto r = switch_read_reg(1);

	while(1)
	{
		__WFI();

		if (ENET->RDAR == 0)
			__BKPT();

		size_t frame_size;
		if (uint8_t* frame = enet_read_get_buffer(&frame_size))
		{
			debug_printf ("%02X%02X%02X%02X%02X%02X %02X%02X%02X%02X%02X%02X %02X%02X ...(%d bytes)\r\n",
				frame[0], frame[1], frame[2], frame[3], frame[4], frame[5],
				frame[6], frame[7], frame[8], frame[9], frame[10], frame[11],
				frame[12], frame[13], frame_size);

			enet_read_release_buffer(frame);
		}

	}
}
