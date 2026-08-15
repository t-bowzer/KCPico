#include "factory.h"
#include "adapters_null.h"

#ifdef KEYBCHORD_NATIVE
#include "storage_stub.h"
#else
#include "input_usbhost.h"
#include "lcd_hd44780.h"
#include "midi_out_uart.h"
#include "storage_littlefs.h"
#endif


Adapters createAdapters() {
    Adapters a;

#ifdef KEYBCHORD_NATIVE
    a.input   = std::make_unique<NullInputAdapter>();
    a.midiOut = std::make_unique<NullMidiOutAdapter>();
    a.lcd     = std::make_unique<NullLcdAdapter>();
    a.storage = std::make_unique<StorageStub>();
#else
    {
        auto input = std::make_unique<InputUsbHost>();
        if (input->begin()) {
            a.input = std::move(input);
        } else {
            a.input = std::make_unique<NullInputAdapter>();
        }
    }

    {
        auto midi = std::make_unique<MidiOutUart>();
        if (midi->begin()) {
            a.midiOut = std::move(midi);
        } else {
            a.midiOut = std::make_unique<NullMidiOutAdapter>();
        }
    }

    {
        auto lcd = std::make_unique<LcdHd44780>();
        if (lcd->begin()) {
            a.lcd = std::move(lcd);
        } else {
            a.lcd = std::make_unique<NullLcdAdapter>();
        }
    }

    {
        auto storage = std::make_unique<StorageLittleFs>();
        if (storage->begin()) {
            a.storage = std::move(storage);
        } else {
            a.storage = std::make_unique<StubStorageAdapter>();
        }
    }
#endif

    return a;
}


