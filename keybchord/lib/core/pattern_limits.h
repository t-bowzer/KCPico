#pragma once

#include <cstddef>
#include <cstdint>

// Shared limits for the on-device rhythm/bass/chord pattern editors (M17).
// The same values are mirrored by the configurator validator.
namespace pattern_limits {

enum class Kind : uint8_t {
    Rhythm = 0,
    Bass,
    Chord,
};

// Built-in + user patterns per kind.
constexpr int MAX_PATTERNS_PER_KIND = 32;

// Combined rhythm/bass/chord pattern-file budget (built-ins + user files) and
// the portion reserved for user-created files.
constexpr size_t ALL_BUDGET_BYTES  = 72u * 1024u;
constexpr size_t USER_BUDGET_BYTES = 48u * 1024u;

// Per-file caps for on-device-created patterns.
constexpr size_t RHYTHM_FILE_MAX_BYTES = 8u * 1024u;
constexpr size_t BASS_FILE_MAX_BYTES   = 4u * 1024u;
constexpr size_t CHORD_FILE_MAX_BYTES  = 4u * 1024u;

constexpr size_t fileMaxBytes(Kind kind) {
    switch (kind) {
        case Kind::Rhythm: return RHYTHM_FILE_MAX_BYTES;
        case Kind::Bass:   return BASS_FILE_MAX_BYTES;
        case Kind::Chord:  return CHORD_FILE_MAX_BYTES;
    }
    return 0;
}

// True when a new pattern can be added to a kind that currently holds
// `currentTotal` built-in + user patterns.
constexpr bool canAddPattern(int currentTotal) {
    return currentTotal < MAX_PATTERNS_PER_KIND;
}

// True when a serialized pattern file fits its per-kind cap.
constexpr bool fileSizeAllowed(Kind kind, size_t bytes) {
    return bytes <= fileMaxBytes(kind);
}

// True when the combined and user-only pattern totals fit the budget.
constexpr bool budgetAllowed(size_t allBytes, size_t userBytes) {
    return allBytes <= ALL_BUDGET_BYTES && userBytes <= USER_BUDGET_BYTES;
}

constexpr bool allBudgetAllowed(size_t allBytes) {
    return allBytes <= ALL_BUDGET_BYTES;
}

constexpr bool userBudgetAllowed(size_t userBytes) {
    return userBytes <= USER_BUDGET_BYTES;
}

} // namespace pattern_limits
