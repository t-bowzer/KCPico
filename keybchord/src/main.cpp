#include <Arduino.h>
#include "pins.h"
#include "factory.h"
#include "config.h"
#include "state.h"
#include "midi_router.h"
#include "midi_event_queue.h"
#include "midimsg.h"
#include "chord_engine.h"
#include "strum_engine.h"
#include "rhythm_engine.h"
#include "debug_log.h"

#if defined(ARDUINO_ARCH_RP2040)
#include "pico/time.h"  // time_us_64() (64-bit monotonic; micros() wraps at ~71 min)
#endif

static Adapters        g_adapters;
static StateManager    g_state;
static MidiRouter*     g_router = nullptr;
static ChordEngine*    g_chordEngine = nullptr;
static StrumEngine*    g_strumEngine = nullptr;
static MidiEventQueue  g_rhythmQueue;
static RhythmEngine*   g_rhythmEngine = nullptr;

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
    g_rhythmEngine = new RhythmEngine(g_state, g_rhythmQueue);
    g_rhythmEngine->setPatterns(loadRhythmPatterns(storage));

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
        g_rhythmEngine->handleKeyEvent(ev, now_us);
    }

    g_chordEngine->update(nowUs());
    g_strumEngine->update(nowUs());

    // Drain rhythm MIDI events produced on Core 1 (single-threaded UART write).
    MidiMessage msg;
    while (g_rhythmQueue.pop(msg)) {
        g_router->sendRaw(msg);
    }

    // Apply the Scroll Lock BPM LED state (computed on Core 1, applied here).
    if (g_state.ledIndicator.on && now_us >= g_state.ledIndicator.untilUs) {
        g_state.ledIndicator.on = false;
        g_state.ledIndicator.dirty = true;
    }
    if (g_state.ledIndicator.dirty) {
        bool ok = g_adapters.input->setLed(midi::LED_SCROLL_LOCK, g_state.ledIndicator.on);
        logLed(midi::LED_SCROLL_LOCK, g_state.ledIndicator.on, ok);
        g_state.ledIndicator.dirty = false;
    }

    // Push any queued MIDI bytes out of the UART FIFO before the next poll so a
    // burst of note-offs (e.g. a rapid strum sweep) is never left stranded.
    g_router->flush();

    delay(10);
}

#if defined(ARDUINO_ARCH_RP2040)
void setup1() {
}

void loop1() {
    // Core 1: rhythm scheduler/clock (deadline-driven; injectable time).
    if (g_rhythmEngine) {
        g_rhythmEngine->update(nowUs());
    }
    delay(1);
}
#endif
