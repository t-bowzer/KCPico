#ifndef __uart_H__
#define __uart_H__

#define MSG_TYPE_CONNECTED      0x01
#define MSG_TYPE_DISCONNECTED   0x02
#define MSG_TYPE_ERROR          0x03
#define MSG_TYPE_DEVICE_POLL    0x04
#define MSG_TYPE_DEVICE_STRING  0x05
#define MSG_TYPE_DEVICE_INFO    0x06
#define MSG_TYPE_HID_INFO       0x07
#define MSG_TYPE_STARTUP        0x08

// Host->CH559 command byte from the RP2040. 0xED followed by one byte (the
// keyboard LED output report: bit0=NumLock, bit1=CapsLock, bit2=ScrollLock)
// triggers a SET_REPORT on the enumerated keyboard.
#define CH559_CMD_LED           0xED

void processUart();
void sendHidPollMSG(unsigned char msgtype, unsigned short length, unsigned char type, unsigned char device, unsigned char endpoint, unsigned char __xdata *msgbuffer,unsigned char idVendorL,unsigned char idVendorH,unsigned char idProductL,unsigned char idProductH);
void sendProtocolMSG(unsigned char msgtype, unsigned short length, unsigned char type, unsigned char device, unsigned char endpoint, unsigned char __xdata *msgbuffer);

#endif