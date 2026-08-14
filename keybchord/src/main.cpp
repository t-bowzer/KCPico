#include <Arduino.h>
#include "pins.h"
#include "factory.h"
#include "config.h"
#include "state.h"
#include "midi_router.h"
#include "debug_log.h"

static Adapters       g_adapters;
static StateManager   g_state;
static MidiRouter*    g_router = nullptr;

void setup() {
    logInit();

    g_adapters = createAdapters();

    auto& storage = *g_adapters.storage;
    g_state.config = AppConfig::load(storage);

    g_router = new MidiRouter(*g_adapters.midiOut, g_state);

    logInfo("KeybChord Pico ready");
}

void loop() {
    if (!g_adapters.input) return;

    auto events = g_adapters.input->poll();
    for (const auto& ev : events) {
        logKeyEvent(ev.hid_usage, ev.pressed, ev.modifiers);

        if (ev.pressed) {
            g_router->sendTestNote();
            logInfo("Test note sent (C4 on channel 1)");
        }
    }

    delay(10);
}

#if defined(ARDUINO_ARCH_RP2040)
void setup1() {
}

void loop1() {
    delay(1000);
}
#endif
