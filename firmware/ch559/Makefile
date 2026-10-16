# CH559 USB-host firmware (fork of atc1441/CH559sdccUSBHost)
#
# Build: `make`  — requires SDCC (>= 4.x). On Debian/Ubuntu: `apt install sdcc`.
# Outputs: ch559.hex (Intel HEX) and ch559.bin (raw binary, for ch55xtool).

PROJECT   := ch559
SDCC      ?= sdcc
PACKIHX   ?= packihx
SDOBJCOPY ?= sdobjcopy

# Memory model / layout mirrors upstream compile.bat.
MODEL     := --model-large
XRAM_SIZE := 0x0800
XRAM_LOC  := 0x0600
CODE_SIZE := 0xEFFF
FREQ_SYS  := 48000000

CFLAGS := -mmcs51 $(MODEL) --xram-size $(XRAM_SIZE) --xram-loc $(XRAM_LOC) \
          --code-size $(CODE_SIZE) -DFREQ_SYS=$(FREQ_SYS) -I.

SRCS := main.c util.c USBHost.c uart.c
RELS := $(SRCS:.c=.rel)
ARTIFACTS := $(PROJECT).ihx $(PROJECT).hex $(PROJECT).bin

.PHONY: all clean

all: $(PROJECT).hex $(PROJECT).bin

%.rel: %.c CH559.h compiler.h
	$(SDCC) -c $(CFLAGS) $<

$(PROJECT).ihx: $(RELS)
	$(SDCC) $(CFLAGS) -o $(PROJECT).ihx $(RELS)

$(PROJECT).hex: $(PROJECT).ihx
	$(PACKIHX) $(PROJECT).ihx > $(PROJECT).hex

$(PROJECT).bin: $(PROJECT).hex
	$(SDOBJCOPY) -I ihex -O binary $(PROJECT).hex $(PROJECT).bin

clean:
	rm -f $(RELS) $(ARTIFACTS) *.asm *.lst *.map *.mem *.lk *.rst *.sym *.adb
