#include <Arduino.h>
#include "pins.h"
#include "factory.h"
#include "config.h"
#include "keymap.h"
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
#include "perf.h"
#include "debug_log.h"

#if defined(ARDUINO_ARCH_RP2040)
#include "pico/time.h"  // time_us_64() (64-bit monotonic; micros() wraps at ~71 min)
#endif

static Adapters        g_adapters;
static StateManager    g_state;
static KeymapResolver  g_keymap;
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

static uint8_t ledUsageFor(const AppConfig& cfg) {    switch (cfg.led_indicator) {
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
static bool     g_wasConnected = false;  // keyboard hot-plug edge tracking
static bool     g_hotplugArmed = false;  // skip the first loop's false edge

// NFR-1 / NFR-2 instrumentation: Core 0 loop-processing time (an upper bound on
// key-to-MIDI latency), per-event key-to-MIDI latency, and the Core 1 clock
// tick jitter (read back via the rhythm engine). Summarized over USB-CDC every
// kPerfLogIntervalUs.
static PerfStats g_latencyStats;      // Core 0 loop-processing time
static PerfStats g_keyLatencyStats;   // HID report receipt -> MIDI dispatch
static uint64_t  g_lastPerfLogUs = 0;
static constexpr uint64_t kPerfLogIntervalUs = 5000000ULL;  // 5 s

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

// Esc (main menu) and keyboard hot-plug disconnect: release chord + strum
// notes only — the rhythm and beat LED are left untouched.
static void cancelChordStrum() {
    if (g_chordEngine) g_chordEngine->allNotesOff();
    if (g_strumEngine) g_strumEngine->allNotesOff();
    g_state.allNotesOff();
}

// Panic (FR-C11): cancel chord/strum, disable the rhythm (it stays off until
// F7), flood All-Sound-Off + All-Notes-Off on all 16 channels, cancel any
// transient UI, and force the beat LED off for the current pulse (it resumes on
// the next beat — the master clock keeps running).
static void handlePanic() {
    cancelChordStrum();
    g_state.pendingRhythm.enabled = false;   // drums stop and stay stopped
    if (g_router) g_router->panic();         // CC120 + CC123 on all 16 channels

    if (g_display) g_display->cancel();
    g_state.cursorActive = false;
    g_state.editMenu = EditMenu::None;
    g_state.editParam = 0;

    g_ledOn = false;
    g_ledDirty = true;
    logInfo("Panic: all-sound-off + all-notes-off (16ch)");
}

void setup() {
    logInit();

    g_adapters = createAdapters();

    auto& storage = *g_adapters.storage;
    g_state.config = AppConfig::load(storage);

    // Apply the run-time logging toggles (NFR-8) from config before anything logs.
    g_debugLogEnabled = g_state.config.debug_log_enabled;
    g_midiMonitorEnabled = g_state.config.midi_monitor_enabled;

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

    // NFR-5: enable the hardware watchdog to recover from a wedged USB stack.
    // Fed on both cores; a 1 s timeout is generous enough for boot/littlefs.
#if defined(ARDUINO_ARCH_RP2040)
    rp2040.wdt_begin(1000);
#endif
}

void loop() {
    if (!g_adapters.input) return;

    uint64_t now_us = nowUs();

    // NFR-5 hot-plug: detect a keyboard disconnect and release any stuck notes.
    // Prevents the beat-LED SET_REPORT retry from spinning while unmounted.
    bool inputConnected = g_adapters.input->connected();
    if (g_hotplugArmed) {
        if (!inputConnected && g_wasConnected) {
            logWarn("Keyboard disconnected: releasing notes");
            cancelChordStrum();
            g_ledDirty = false;
        } else if (inputConnected && !g_wasConnected) {
            logInfo("Keyboard reconnected");
        }
    }
    g_hotplugArmed = true;
    g_wasConnected = inputConnected;

    auto events = g_adapters.input->poll();
    for (const auto& ev : events) {
        logKeyEvent(ev.hid_usage, ev.pressed, ev.modifiers);

        if (ev.pressed) {
            ActionType t = g_keymap.resolve(ev.hid_usage, ev.modifiers).type;
            // Super+Esc: full panic, works from any state (prompt, cursor, menu).
            if (t == ActionType::Panic) {
                handlePanic();
                continue;
            }
            // Esc (no super) in the plain main menu cancels chord+strum sounds;
            // in an edit menu / cursor / prompt it keeps its existing meaning
            // (exit menu, exit cursor, cancel prompt) via the engines below.
            if (t == ActionType::ClearEdit &&
                g_state.editMenu == EditMenu::None &&
                !g_state.cursorActive &&
                !g_presetEngine->promptActive()) {
                cancelChordStrum();
                logInfo("Esc: chord/strum cancelled");
                continue;
            }
        }

        bool consumed = g_presetEngine->handleKeyEvent(ev, now_us);
        if (!consumed) {
            consumed = g_editEngine->handleKeyEvent(ev, now_us);
        }
        if (!consumed) {
            g_chordEngine->handleKeyEvent(ev, now_us);
            g_strumEngine->handleKeyEvent(ev, now_us);
        }

        // NFR-1: per-event latency from HID report receipt to dispatch (events
        // from the null adapter or tests carry received_us == 0 and are skipped).
        if (ev.received_us != 0) {
            uint64_t dispatch_us = nowUs();
            g_keyLatencyStats.record(
                static_cast<uint32_t>(dispatch_us - ev.received_us));
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
    if (g_ledDirty && inputConnected) {
        if (applyLedState(g_ledOn)) {
            g_ledDirty = false;   // retry on the next loop if not queued
        }
    }

    // Push any queued MIDI bytes out of the UART FIFO before the next poll so a
    // burst of note-offs (e.g. a rapid strum sweep) is never left stranded.
    g_router->flush();

    // NFR-1/NFR-2 instrumentation: summarize Core 0 loop time, per-event
    // key-to-MIDI latency, and Core 1 clock jitter over CDC every 5 s.
    uint64_t end_us = nowUs();
    g_latencyStats.record(static_cast<uint32_t>(end_us - now_us));
    if (end_us - g_lastPerfLogUs >= kPerfLogIntervalUs) {
        g_lastPerfLogUs = end_us;
        logPerf("core0-loop-us", g_latencyStats.count(), g_latencyStats.minUs(),
                g_latencyStats.maxUs(), g_latencyStats.avgUs());
        g_latencyStats.reset();

        logPerf("key-to-midi-us", g_keyLatencyStats.count(),
                g_keyLatencyStats.minUs(), g_keyLatencyStats.maxUs(),
                g_keyLatencyStats.avgUs());
        g_keyLatencyStats.reset();

        PerfStats jitter;
        if (g_rhythmEngine) g_rhythmEngine->jitterStats(jitter, true);
        logPerf("clock-jitter-us", jitter.count(), jitter.minUs(),
                jitter.maxUs(), jitter.avgUs());
    }

    delay(10);
#if defined(ARDUINO_ARCH_RP2040)
    rp2040.wdt_reset();
#endif
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
    rp2040.wdt_reset();
}
#endif
