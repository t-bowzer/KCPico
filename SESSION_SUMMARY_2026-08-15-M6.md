# KeybChord Pico — Session 2026-08-15 (M6) Summary

## Goal: M6 — Display Manager (roadmap §6 line 404; spec §4.6, AC-8), plus a rework of parameter editing into menus

Two pieces landed today:

1. **LCD1602 Display Manager** (idle screen, transient param-edit feedback, prompts).
2. A **menu-driven rework of parameter editing** requested mid-session: no
   modifier-key combos — instead, three edit menus entered by dedicated keys,
   where F-keys select a parameter and `+`/`-` change its value.

## Result: SUCCESS

- **Native tests:** `pio test -e native` → **190/190 passing** (was 164 at M5).
- **Pico build:** `pio run -e pico -t buildunified` → SUCCESS
  (`firmware_with_fs.uf2`, RAM 12.5%, Flash 9.6%).
- **On-device (reported by user):** all display + menu steps pass; chord and
  strum keys remain live inside menus so edits are heard immediately.

## Parameter-edit model (new)

- **Chord Edit** menu — toggled by the **Menu** key (`0x65`), `F10` as a
  fallback for keyboards without one: F1 Octave, F2 Mode, F3 Voicing,
  F4 Duration, F5 Velocity, F6 Pan, F7/F8/F9 Add9/11/13.
- **Strum Edit** menu — toggled by **Alt**: F1 Octave, F2 Duration, F3 Velocity,
  F4 Layout (Full/Limited).
- **Rhythm Edit** menu — toggled by **Ctrl**: F1 Tempo, F2 Swing, F3 Pattern,
  F4 Mute, F5 Enable, F6 Clock Out, F7 Beat LED.
- Inside a menu: F-keys select a parameter (bottom line shows its name);
  `+`/`-` (or `Page Up`/`Page Down`) change it, showing `<full name> <value>`
  transiently, reverting to the menu after `display_revert_timeout_ms`.
  `Esc` returns to the main menu; re-pressing the toggle key exits; a different
  toggle switches menus.
- **Main-menu single-key shortcuts:** `F1` mode cycle, `F5` voicing,
  `←/↓/→` add9/11/13, number-row `=`/`-` = chord octave, `Keypad +`/`-` =
  strum octave, `F7/F8/F9` rhythm on/pattern/mute, `Page Up/Down` tempo.
- **Presets move to Super combos** (`Super+Home/End`, `Super+1..8`,
  `Super+Insert/Delete`) since Ctrl is now a menu toggle; `Super` is otherwise
  free (`Super+Esc` panic only). This is the M7 plan, not yet implemented.

## New files

```
lib/core/param_edit.h/.cpp      # ParamId registry, menu tables, names, values,
                                  paramStep/paramCycle (single source of truth)
lib/engines/edit_engine.h/.cpp   # owns menu state + all parameter mutation;
                                  routes F-keys/+/-/Esc; fires mode/pattern hooks
lib/core/naming.h/.cpp           # playModeShort / voicingModeName / rhythmShortCode
lib/engines/display_manager.h/.cpp # idle + menu + transient value + prompt screens
lib/hw/lcd_hd44780.h/.cpp        # hd44780/PCF8574 I2C, probe 0x27->0x3F, null fallback
test/test_param_edit.cpp
test/test_edit_engine.cpp
test/test_naming.cpp
test/test_display_manager.cpp
```

## Modified files

- `lib/core/params.h` — `EditMenu` enum; swing bounds already signed `-75..+75`.
- `lib/core/state.*` — `EditTarget` → `EditMenu editMenu` + `int editParam`.
- `lib/core/keymap.*` — menu-toggle actions; octave split (number-row vs keypad);
  removed `StrumOctave/Duration/Velocity/LimitedToggle/Swing*/Clock/Led` combo actions.
- `lib/engines/chord_engine.*` — playback only + `onModeChanged()`.
- `lib/engines/strum_engine.*` — strum keys only (`stepEdit`/EditTarget removed).
- `lib/engines/rhythm_engine.*` — scheduler only + `onPatternChanged()`.
- `lib/hw/factory.cpp` — real `LcdHd44780` on pico with null fallback.
- `src/main.cpp` — `EditEngine` wiring + consume/forward event routing.
- `platformio.ini` — `edit_engine.*`, `display_manager.*` in native filter.
- `KeybChord_Pico_Spec.md` — §4.6 (FR-D2 menus), §5.3–5.8 (menu key map,
  Super presets), §7.3 (signed swing + clock 0xF8-only), §9 (swing −75..+75),
  §13 (AC-16/AC-23) updated.

## Design decisions

- **Central param registry** (`param_edit`) so future settings are added by
  extending one table, not reworking mappings.
- **One editor** (`EditEngine`) owns all param mutation; chord/strum/rhythm
  engines keep only playback, with `onModeChanged`/`onPatternChanged` hooks for
  the two side effects (release latched chord; adopt pattern swing).
- **Event-driven display**: `EditEngine` drives `showMenu/selectParam/showValue`;
  the diff-snapshot from M6 was removed.
- **Chord/strum keys stay live in menus** (forwarded) for immediate audible
  feedback; everything else is consumed while a menu is open.
- **Swing is signed** (−75..+75, positive laid-back, negative rushed) — the spec
  now documents this and the clock's 0xF8-only (no Start/Stop) behavior.

## Verification approach

- Native: GoogleTest; `pio test -e native` green (190).
- On-device: `pio run -e pico -t buildunified` → drag `firmware_with_fs.uf2`
  (BOOTSEL) → monitor 115200.

## Next steps

1. **M7 — Presets & banks** (roadmap §6 line 409; spec §4.4, AC-7/19/20/21):
   - JSON load/save on LittleFS (foundation already in `presets.cpp`);
     80-slot navigation (`Home/End`, `Super+Home/End`, `Super+1..8`).
   - Save/clear with confirm prompt (`Super+Insert/Delete`) wired to
     `DisplayManager::showPrompt` + `display_prompt_ms` auto-cancel + play-key
     cancel (FR-P10).
   - Per-function channels (chord 1 / strum 2 / rhythm 10), defaults for
     uninitialized/cleared slots (FR-P9/P12), and real dirty-state tracking
     (`state.dirty`, FR-P11) to drive the `*` marker.
2. **M8 — Polish** (roadmap §6 line 414; spec §13 + NFRs): panic
   (`Super+Esc` → CC120/CC123 all channels + clear state), latency/jitter
   measurement, robustness/hot-plug, config validation/fallback hardening,
   logging/MIDI-monitor hardening, full test suite, optional watchdog.
3. **Roadmap sync-up (follow-up):** `KeybChord_Pico_Roadmap.md` §2.4/§5.2/§6
   still describe the old `Ctrl`/`Alt`/`Super` combo key map and M7 `Ctrl+…`
   preset keys; update them to match the new menu model and Super-based presets.
4. **Cross-core shared state** (`pendingRhythm`/`RhythmClock` plain volatiles)
   hardening remains deferred to M8 (carried from M5).
