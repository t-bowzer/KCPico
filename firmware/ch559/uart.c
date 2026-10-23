
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "CH559.h"
#include "util.h"
#include "uart.h"
#include "USBHost.h"

static unsigned char rxExpectLed = 0;

// Reads UART1 command bytes from the RP2040. The only command we accept is the
// LED SET_REPORT relay: 0xED followed by the one-byte LED output report. All
// other bytes are ignored (the upstream 'kb' bootloader command is intentionally
// dropped — bootloader entry on this fork is via the BOOT button / DP pull-up).
void processUart(){
    while (SER1_LSR & bLSR_DATA_RDY) {
        unsigned char c = SER1_FIFO;
        if (rxExpectLed) {
            rxExpectLed = 0;
            setHidKeyboardLed(c);
        } else if (c == CH559_CMD_LED) {
            rxExpectLed = 1;
        }
    }
}

void sendProtocolMSG(unsigned char msgtype, unsigned short length, unsigned char type, unsigned char device, unsigned char endpoint, unsigned char __xdata *msgbuffer){
    unsigned short i;
    putchar(0xFE);	
	putchar(length);
	putchar((unsigned char)(length>>8));
	putchar(msgtype);
	putchar(type);
	putchar(device);
	putchar(endpoint);
	putchar(0);
	putchar(0);
	putchar(0);
	putchar(0);
	for (i = 0; i < length; i++)
	{
		putchar(msgbuffer[i]);
	}
	putchar('\n');
}

void sendHidPollMSG(unsigned char msgtype, unsigned short length, unsigned char type, unsigned char device, unsigned char endpoint, unsigned char __xdata *msgbuffer,unsigned char idVendorL,unsigned char idVendorH,unsigned char idProductL,unsigned char idProductH){
    unsigned short i;
    putchar(0xFE);	
	putchar(length);
	putchar((unsigned char)(length>>8));
	putchar(msgtype);
	putchar(type);
	putchar(device);
	putchar(endpoint);
	putchar(idVendorL);
	putchar(idVendorH);
	putchar(idProductL);
	putchar(idProductH);
	for (i = 0; i < length; i++)
	{
		putchar(msgbuffer[i]);
	}
	putchar('\n');
}