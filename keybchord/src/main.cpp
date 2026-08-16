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
#include "edit_engine.h"
#include "display_manager.h"
#include "preset_engine.h"
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
static DisplayManager* g_display = nullptr;
static EditEngine*     g_editEngine = nullptr;
static PresetEngine*   g_presetEngine = nullptr;

static inline uint64_t nowUs() {
#if defined(ARDUINO_ARCH_RP2040)
    return time_us_64();
#else
    return micros();
#endif
}

static uint8_t ledUsageFor(const AppConfig& cfg) {
    switch (cfg.led_indicator) {
        case LedTarget::CapsLock: return midi::LED_CAPS_LOCK;
        case LedTarget::NumLock:  return midi::LED_NUM_LOCK;
        case LedTarget::ScrollLock:
        case LedTarget::All:
        default:                  return midi::LED_SCROLL_LOCK;
    }
}

// Beat-LED state owned entirely by Core 0 (the Core 1 scheduler only posts a
// "flash" request). Keeping the on/off deadline local avoids any cross-core
// torn read that could leave the keyboard LED stuck on.
static bool     g_ledOn       = false;
static uint64_t g_ledOffAtUs  = 0;
static bool     g_ledDirty    = false;

// Sets every target LED on/off; returns true if all SET_REPORTs were queued.
static bool applyLedState(bool on) {
    bool allOk = true;
    if (g_state.config.led_indicator == LedTarget::All) {
        static const uint8_t kAllLeds[] = {
            midi::LED_NUM_LOCK, midi::LED_CAPS_LOCK, midi::LED_SCROLL_LOCK,
        };
        for (uint8_t led : kAllLeds) {
            bool ok = g_adapters.input->setLed(led, on);
            logLed(led, on, ok);
            if (!ok) allOk = false;
        }
    } else {
        uint8_t led = ledUsageFor(g_state.config);
        bool ok = g_adapters.input->setLed(led, on);
        logLed(led, on, ok);
        allOk = ok;
    }
    return allOk;
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

    g_display = new DisplayManager(g_state, *g_adapters.lcd);
    g_display->update(nowUs());

    g_editEngine = new EditEngine(g_state, *g_display);
    g_editEngine->setModeChangedCallback([]() { g_chordEngine->onModeChanged(); });
    g_editEngine->setPatternChangedCallback([]() { g_rhythmEngine->onPatternChanged(); });

    g_presetEngine = new PresetEngine(g_state, *g_adapters.storage, *g_display);
    g_presetEngine->setModeChangedCallback([]() { g_chordEngine->onModeChanged(); });
    g_editEngine->setAnyEditCallback([]() { g_presetEngine->recomputeDirty(); });

    g_presetEngine->loadStartupPreset();
    g_display->update(nowUs());

    logInfo("KeybChord Pico ready");
}

void loop() {
    if (!g_adapters.input) return;

    uint64_t now_us = nowUs();

    auto events = g_adapters.input->poll();
    for (const auto& ev : events) {
        logKeyEvent(ev.hid_usage, ev.pressed, ev.modifiers);
        bool consumed = g_presetEngine->handleKeyEvent(ev, now_us);
        if (!consumed) {
            consumed = g_editEngine->handleKeyEvent(ev, now_us);
        }
        if (!consumed) {
            g_chordEngine->handleKeyEvent(ev, now_us);
            g_strumEngine->handleKeyEvent(ev, now_us);
        }
    }

    g_chordEngine->update(nowUs());
    g_strumEngine->update(nowUs());
    g_presetEngine->update(nowUs());
    g_editEngine->update(nowUs());

    // Render the LCD idle/edit/prompt frame from the live state.
    if (g_display) {
        g_display->update(nowUs());
    }

    // Drain rhythm MIDI events produced on Core 1 (single-threaded UART write).
    MidiMessage msg;
    while (g_rhythmQueue.pop(msg)) {
        g_router->sendRaw(msg);
    }

    // Beat LED (FR-R8): Core 1 posts a flash request; Core 0 owns the on/off
    // state — deterministic off after led_flash_ms, retried until the report
    // is queued so a dropped SET_REPORT can't leave the LED stuck on.
    if (g_state.ledIndicator.flash) {
        g_state.ledIndicator.flash = false;
        g_ledOn = true;
        g_ledOffAtUs = now_us + static_cast<uint64_t>(g_state.config.led_flash_ms) * 1000ULL;
        g_ledDirty = true;
    }
    if (g_ledOn && now_us >= g_ledOffAtUs) {
        g_ledOn = false;
        g_ledDirty = true;
    }
    if (g_ledDirty) {
        if (applyLedState(g_ledOn)) {
            g_ledDirty = false;   // retry on the next loop if not queued
        }
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
