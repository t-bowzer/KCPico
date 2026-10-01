#pragma once

// Gated CDC test-command hook (M11 hardware test). Enabled only when built with
// -DKEYBCHORD_TEST_HOOK (the [env:testhook] environment). It reads newline-
// terminated ASCII commands from the native-USB CDC port and drives the
// engines deterministically so the USB-MIDI/DIN outputs can be exercised by a
// host-side script instead of a human pressing keys.
//
// Command set (case-insensitive):
//   PING                      -> PONG
//   VERSION                   -> KEYBCHORD-TESTHOOK 1
//   NOTEON <ch> <note> <vel>  -> note-on via the MIDI router
//   NOTEOFF <ch> <note>       -> note-off via the MIDI router
//   CC <ch> <cc> <val>        -> control change via the MIDI router
//   KEY <usage> <down|up>     -> synthesize a key event through chord+strum
//   USB <on|off>              -> set config.usb_midi_enabled
//   CLOCK <on|off>            -> set config.midi_clock_enabled
//   RHYTHM <on|off>           -> set pendingRhythm.enabled
//   PANIC                     -> router panic (all-sound/notes-off all channels)
//
// Every response line is prefixed with "[TH] " so a host script can filter them
// out of the debug log stream.

#include <Arduino.h>

#include <cctype>
#include <cstdlib>
#include <string>

#include "base.h"
#include "chord_engine.h"
#include "midi_router.h"
#include "state.h"
#include "strum_engine.h"

class TestHook {
public:
    TestHook(StateManager& state, MidiRouter& router,
             ChordEngine* chord, StrumEngine* strum)
        : state_(state), router_(router), chord_(chord), strum_(strum) {}

    void poll(uint64_t now_us) {
        while (Serial.available() > 0) {
            int c = Serial.read();
            if (c < 0) break;
            if (c == '\n' || c == '\r') {
                if (!line_.empty()) {
                    dispatch(line_, now_us);
                    line_.clear();
                }
            } else if (line_.size() < kMaxLine) {
                line_ += static_cast<char>(c);
            }
        }
    }

private:
    static constexpr size_t kMaxLine = 64;

    StateManager& state_;
    MidiRouter& router_;
    ChordEngine* chord_;
    StrumEngine* strum_;
    std::string line_;

    void respond(const char* s) {
        Serial.print("[TH] ");
        Serial.println(s);
    }

    static void toLower(std::string& s) {
        for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    // Tokenize on whitespace.
    static std::vector<std::string> split(const std::string& s) {
        std::vector<std::string> out;
        std::string cur;
        for (char c : s) {
            if (std::isspace(static_cast<unsigned char>(c))) {
                if (!cur.empty()) { out.push_back(cur); cur.clear(); }
            } else {
                cur += c;
            }
        }
        if (!cur.empty()) out.push_back(cur);
        return out;
    }

    static bool parseU8(const std::string& s, int& out) {
        if (s.empty()) return false;
        for (char c : s) {
            if (!std::isdigit(static_cast<unsigned char>(c))) return false;
        }
        out = std::atoi(s.c_str());
        return out >= 0 && out <= 255;
    }

    static bool onOff(const std::string& s, bool& out) {
        if (s == "on" || s == "1" || s == "true")  { out = true;  return true; }
        if (s == "off" || s == "0" || s == "false") { out = false; return true; }
        return false;
    }

    void dispatch(const std::string& line, uint64_t now_us) {
        std::vector<std::string> t = split(line);
        if (t.empty()) { respond("ERR empty"); return; }

        std::string cmd = t[0];
        toLower(cmd);

        if (cmd == "ping") {
            respond("PONG");
        } else if (cmd == "version") {
            respond("KEYBCHORD-TESTHOOK 1");
        } else if (cmd == "noteon" && t.size() == 4) {
            int ch, note, vel;
            if (parseU8(t[1], ch) && parseU8(t[2], note) && parseU8(t[3], vel) &&
                ch >= 1 && ch <= 16) {
                router_.noteOn(static_cast<uint8_t>(ch), static_cast<uint8_t>(note),
                               static_cast<uint8_t>(vel));
                respond("OK");
            } else {
                respond("ERR NOTEON ch note vel");
            }
        } else if (cmd == "noteoff" && t.size() == 3) {
            int ch, note;
            if (parseU8(t[1], ch) && parseU8(t[2], note) && ch >= 1 && ch <= 16) {
                router_.noteOff(static_cast<uint8_t>(ch), static_cast<uint8_t>(note));
                respond("OK");
            } else {
                respond("ERR NOTEOFF ch note");
            }
        } else if (cmd == "cc" && t.size() == 4) {
            int ch, cc, val;
            if (parseU8(t[1], ch) && parseU8(t[2], cc) && parseU8(t[3], val) &&
                ch >= 1 && ch <= 16) {
                router_.cc(static_cast<uint8_t>(ch), static_cast<uint8_t>(cc),
                           static_cast<uint8_t>(val));
                respond("OK");
            } else {
                respond("ERR CC ch cc val");
            }
        } else if (cmd == "key" && t.size() == 3) {
            int usage;
            bool down;
            std::string dir = t[2];
            toLower(dir);
            if (parseU8(t[1], usage) && (dir == "down" || dir == "up")) {
                down = (dir == "down");
                KeyEvent ev;
                ev.hid_usage = static_cast<uint8_t>(usage);
                ev.pressed = down;
                ev.modifiers = 0;
                ev.received_us = now_us;
                if (chord_) chord_->handleKeyEvent(ev, now_us);
                if (strum_) strum_->handleKeyEvent(ev, now_us);
                respond("OK");
            } else {
                respond("ERR KEY usage down|up");
            }
        } else if (cmd == "usb" && t.size() == 2) {
            bool v;
            if (onOff(t[1], v)) {
                state_.config.usb_midi_enabled = v;
                respond("OK");
            } else {
                respond("ERR USB on|off");
            }
        } else if (cmd == "clock" && t.size() == 2) {
            bool v;
            if (onOff(t[1], v)) {
                state_.config.midi_clock_enabled = v;
                respond("OK");
            } else {
                respond("ERR CLOCK on|off");
            }
        } else if (cmd == "rhythm" && t.size() == 2) {
            bool v;
            if (onOff(t[1], v)) {
                state_.pendingRhythm.enabled = v;
                respond("OK");
            } else {
                respond("ERR RHYTHM on|off");
            }
        } else if (cmd == "panic") {
            router_.panic();
            respond("OK");
        } else {
            respond("ERR unknown");
        }
    }
};
