#include <Arduino.h>
#include "pins.h"
#include "factory.h"
#include "config.h"
#include "state.h"
#include "midi_router.h"
#include "chord_engine.h"
#include "strum_engine.h"
#include "debug_log.h"

#if defined(ARDUINO_ARCH_RP2040)
#include "pico/time.h"  // time_us_64() (64-bit monotonic; micros() wraps at ~71 min)
#endif

static Adapters       g_adapters;
static StateManager   g_state;
static MidiRouter*    g_router = nullptr;
static ChordEngine*   g_chordEngine = nullptr;
static StrumEngine*   g_strumEngine = nullptr;

static inline uint64_t nowUs() {
#if defined(ARDUINO_ARCH_RP2040)
    return time_us_64();
#else
    return micros();
#endif
}

void setup() {
    logInit();

    g_adapters = createAdapters();

    auto& storage = *g_adapters.storage;
    g_state.config = AppConfig::load(storage);

    g_router = new MidiRouter(*g_adapters.midiOut, g_state);
    g_chordEngine = new ChordEngine(g_state, *g_router);
    g_strumEngine = new StrumEngine(g_state, *g_router);

    logInfo("KeybChord Pico ready");
}

void loop() {
    if (!g_adapters.input) return;

    uint64_t now_us = nowUs();

    auto events = g_adapters.input->poll();
    for (const auto& ev : events) {
        logKeyEvent(ev.hid_usage, ev.pressed, ev.modifiers);
        g_chordEngine->handleKeyEvent(ev, now_us);
        g_strumEngine->handleKeyEvent(ev, now_us);
    }

    g_chordEngine->update(nowUs());
    g_strumEngine->update(nowUs());

    // Push any queued MIDI bytes out of the UART FIFO before the next poll so a
    // burst of note-offs (e.g. a rapid strum sweep) is never left stranded.
    g_router->flush();

    delay(10);
}

#if defined(ARDUINO_ARCH_RP2040)
void setup1() {
}

void loop1() {
    delay(1000);
}
#endif
