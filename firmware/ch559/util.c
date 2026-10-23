#include "CH559.h"
#include "util.h"

FunctionReference runBootloader = (FunctionReference)0xF400;

#ifndef FREQ_SYS
#define	FREQ_SYS	48000000
#endif 

void initClock()
{
    SAFE_MOD = 0x55;
    SAFE_MOD = 0xAA;

	CLOCK_CFG &= ~MASK_SYS_CK_DIV;
	CLOCK_CFG |= 6; 															  
	PLL_CFG = ((24 << 0) | (6 << 5)) & 255;

    SAFE_MOD = 0xFF;

	delay(7);
}

/**
 * Initialize UART0 port with given boud rate
 * pins: tx = P3.1 rx = P3.0
 * alt != 0 pins: tx = P0.2 rx = P0.3
 */

void initUART0(unsigned long baud, int alt)
{
	unsigned long x;
	if(alt)
	{
		PORT_CFG |= bP0_OC;
		P0_DIR |= bTXD_;
		P0_PU |= bTXD_ | bRXD_;
		PIN_FUNC |= bUART0_PIN_X;
	}

 	SM0 = 0;
	SM1 = 1;
	SM2 = 0;
	REN = 1;
   //RCLK = 0;
    //TCLK = 0;
    PCON |= SMOD;
    x = (((unsigned long)FREQ_SYS / 8) / baud + 1) / 2;

    TMOD = TMOD & ~ bT1_GATE & ~ bT1_CT & ~ MASK_T1_MOD | bT1_M1;
    T2MOD = T2MOD | bTMR_CLK | bT1_CLK;
    TH1 = (256 - x) & 255;
    TR1 = 1;
	TI = 1;
}

/**
 * Initialize UART1 port with the given baud rate.
 * pins: tx = P2.7, rx = P2.6 (RXD1/TXD1 on P2.6/P2.7).
 * Fsys must already be 48 MHz (call initClock() first).
 */
void initUART1(unsigned long baud)
{
    unsigned long x;
    unsigned char x2;

    // 8 data bits, no parity, 1 stop bit.
    SER1_LCR = MASK_U1_WORD_SZ;

    // Baud divisor: x = round(FREQ_SYS / (8 * DIV * baud)), DIV = 1.
    // The CH559 UART1 uses 8x oversampling (not the 16550's 16x), so the
    // divisor is FREQ_SYS/(8*baud); using /16 halves it and doubles the baud.
    x = 10 * FREQ_SYS / baud / 8;
    x2 = (unsigned char)(x % 10);
    x /= 10;
    if (x2 >= 5) x++;

    SER1_LCR |= bLCR_DLAB;
    SER1_DIV = 1;
    SER1_DLM = (unsigned char)(x >> 8);
    SER1_DLL = (unsigned char)(x & 0xFF);
    SER1_LCR &= ~bLCR_DLAB;

    // Pin mode 010: RXD1/TXD1 on P2.6/P2.7. The 3-bit pin-mode field is
    // [RS485EN : PIN_MOD1 : PIN_MOD0] (SER1_IER bits 6:4); "010" selects
    // P2.6/P2.7, so PIN_MOD1 (0x20) must be set (PIN_MOD0 would be "001" =
    // P4.0/P4.4). Written AFTER DLAB is cleared because SER1_IER shares SFR
    // address 0x91 with SER1_DLM.
    SER1_IER = (SER1_IER & ~MASK_U1_PIN_MOD) | bIER_PIN_MOD1;

    // Enable the FIFO and flush both sides.
    SER1_FCR = bFCR_FIFO_EN | bFCR_R_FIFO_CLR | bFCR_T_FIFO_CLR;
}

unsigned char UART0Receive()
{
    while(RI == 0);
    RI = 0;
    return SBUF;
}

void UART0Send(unsigned char b)
{
	SBUF = b;
	while(TI == 0);
	TI = 1;
}

unsigned char UART1Receive()
{
    while((SER1_LSR & bLSR_DATA_RDY) == 0);
    return SER1_FIFO;
}

void UART1Send(unsigned char b)
{
    while((SER1_LSR & bLSR_T_FIFO_EMP) == 0);
    SER1_THR = b;
}

/**
* #define PIN_MODE_INPUT 0
* #define PIN_MODE_INPUT_PULLUP 1
* #define PIN_MODE_OUTPUT 2
* #define PIN_MODE_OUTPUT_OPEN_DRAIN 3
* #define PIN_MODE_OUTPUT_OPEN_DRAIN_2CLK 4
* #define PIN_MODE_INPUT_OUTPUT_PULLUP 5
* #define PIN_MODE_INPUT_OUTPUT_PULLUP_2CLK 6
 */
void pinMode(unsigned char port, unsigned char pin, unsigned char mode)
{
	volatile unsigned char *dir[] = {&P0_DIR, &P1_DIR, &P2_DIR, &P3_DIR};
	volatile unsigned char *pu[] = {&P0_PU, &P1_PU, &P2_PU, &P3_PU};
	switch (mode)
	{
	case PIN_MODE_INPUT: //Input only, no pull up
		PORT_CFG &= ~(bP0_OC << port);
		*dir[port] &= ~(1 << pin);
		*pu[port] &= ~(1 << pin);
		break;
	case PIN_MODE_INPUT_PULLUP: //Input only, pull up
		PORT_CFG &= ~(bP0_OC << port);
		*dir[port] &= ~(1 << pin);
		*pu[port] |= 1 << pin;
		break;
	case PIN_MODE_OUTPUT: //Push-pull output, high and low level strong drive
		PORT_CFG &= ~(bP0_OC << port);
		*dir[port] |= ~(1 << pin);
		break;
	case PIN_MODE_OUTPUT_OPEN_DRAIN: //Open drain output, no pull-up, support input
		PORT_CFG |= (bP0_OC << port);
		*dir[port] &= ~(1 << pin);
		*pu[port] &= ~(1 << pin);
		break;
	case PIN_MODE_OUTPUT_OPEN_DRAIN_2CLK: //Open-drain output, no pull-up, only drives 2 clocks high when the transition output goes from low to high
		PORT_CFG |= (bP0_OC << port);
		*dir[port] |= 1 << pin;
		*pu[port] &= ~(1 << pin);
		break;
	case PIN_MODE_INPUT_OUTPUT_PULLUP: //Weakly bidirectional (standard 51 mode), open drain output, with pull-up
		PORT_CFG |= (bP0_OC << port);
		*dir[port] &= ~(1 << pin);
		*pu[port] |= 1 << pin;
		break;
	case PIN_MODE_INPUT_OUTPUT_PULLUP_2CLK: //Quasi-bidirectional (standard 51 mode), open-drain output, with pull-up, when the transition output is low to high, only drives 2 clocks high
		PORT_CFG |= (bP0_OC << port);
		*dir[port] |= 1 << pin;
		*pu[port] |= 1 << pin;
		break;
	default:
		break;
	}
}
/*
unsigned char getPortAddress(unsigned char port)
{
	const unsigned char portAddr[] = {0x80, 0x90, 0xA0, 0xB0, 0xC0};
	return portAddr[port];
}

unsigned char digitalRead(unsigned char port, unsigned char pin)
{

}
*/
/**
 * stdio printf directed to UART1 using putchar and getchar
 */

int putchar(int c)
{
    while ((SER1_LSR & bLSR_T_FIFO_EMP) == 0);
    SER1_THR = c & 0xFF;
    return c;
}

int getchar() 
{
    while ((SER1_LSR & bLSR_DATA_RDY) == 0);
    return SER1_FIFO;
}

/*******************************************************************************
* Function Name  : delayUs(UNIT16 n)
* Description    : us
* Input          : UNIT16 n
* Output         : None
* Return         : None
*******************************************************************************/ 
void	delayUs(unsigned short n)
{
	while (n) 
	{  // total = 12~13 Fsys cycles, 1uS @Fsys=12MHz
		++ SAFE_MOD;  // 2 Fsys cycles, for higher Fsys, add operation here
#ifdef	FREQ_SYS
#if		FREQ_SYS >= 14000000
		++ SAFE_MOD;
#endif
#if		FREQ_SYS >= 16000000
		++ SAFE_MOD;
#endif
#if		FREQ_SYS >= 18000000
		++ SAFE_MOD;
#endif
#if		FREQ_SYS >= 20000000
		++ SAFE_MOD;
#endif
#if		FREQ_SYS >= 22000000
		++ SAFE_MOD;
#endif
#if		FREQ_SYS >= 24000000
		++ SAFE_MOD;
#endif
#if		FREQ_SYS >= 26000000
		++ SAFE_MOD;
#endif
#if		FREQ_SYS >= 28000000
		++ SAFE_MOD;
#endif
#if		FREQ_SYS >= 30000000
		++ SAFE_MOD;
#endif
#if		FREQ_SYS >= 32000000
		++ SAFE_MOD;
#endif
#if		FREQ_SYS >= 34000000
		++ SAFE_MOD;
#endif
#if		FREQ_SYS >= 36000000
		++ SAFE_MOD;
#endif
#if		FREQ_SYS >= 38000000
		++ SAFE_MOD;
#endif
#if		FREQ_SYS >= 40000000
		++ SAFE_MOD;
#endif
#if		FREQ_SYS >= 42000000
		++ SAFE_MOD;
#endif
#if		FREQ_SYS >= 44000000
		++ SAFE_MOD;
#endif
#if		FREQ_SYS >= 46000000
		++ SAFE_MOD;
#endif
#if		FREQ_SYS >= 48000000
		++ SAFE_MOD;
#endif
#if		FREQ_SYS >= 50000000
		++ SAFE_MOD;
#endif
#if		FREQ_SYS >= 52000000
		++ SAFE_MOD;
#endif
#if		FREQ_SYS >= 54000000
		++ SAFE_MOD;
#endif
#if		FREQ_SYS >= 56000000
		++ SAFE_MOD;
#endif
#endif
		--n;
	}
}

/*******************************************************************************
* Function Name  : delay(UNIT16 n)
* Description    : ms
* Input          : UNIT16 n
* Output         : None
* Return         : None
*******************************************************************************/
void delay(unsigned short n)
{
	while (n) 
	{
		delayUs(1000);
		--n;
	}
}