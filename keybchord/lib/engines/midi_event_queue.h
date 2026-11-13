#pragma once

#include <cstddef>
#include <cstdint>

#include "base.h"


// One queued MIDI message plus the Core 1 enqueue time. Core 0 uses the
// timestamp to measure dispatch latency without adding work to Core 1.
struct QueuedMidi {
    MidiMessage msg;
    uint64_t queued_us;
};

// Lock-free single-producer / single-consumer ring buffer of MIDI messages.
//
// Core 1 (the rhythm scheduler) is the sole producer; Core 0 (the MIDI dispatch
// loop) is the sole consumer. head_/tail_ are monotonically increasing counters
// masked on access, so the classic SPSC full/empty conditions apply without
// wrapping. On the RP2040 (two in-order M0+ cores, no cache) `volatile` is
// sufficient for safe hand-off.
class MidiEventQueue {
public:
    static constexpr size_t CAPACITY = 256;

    bool push(const MidiMessage& msg, uint64_t queued_us = 0) {
        size_t h = head_;
        size_t t = tail_;
        if (h - t >= CAPACITY) {
            overflowCount_++;
            return false;  // full
        }
        buf_[h % CAPACITY] = {msg, queued_us};
        head_ = h + 1;
        return true;
    }

    bool pop(QueuedMidi& out) {
        size_t h = head_;
        size_t t = tail_;
        if (h == t) return false;  // empty
        out = buf_[t % CAPACITY];
        tail_ = t + 1;
        return true;
    }

    bool pop(MidiMessage& out) {
        QueuedMidi queued;
        if (!pop(queued)) return false;
        out = queued.msg;
        return true;
    }

    bool empty() const { return head_ == tail_; }
    size_t size() const { return head_ - tail_; }

    void clear() { tail_ = head_; }

    uint32_t takeOverflowCount() {
        uint32_t n = overflowCount_;
        overflowCount_ = 0;
        return n;
    }

private:
    QueuedMidi buf_[CAPACITY];
    volatile size_t head_ = 0;
    volatile size_t tail_ = 0;
    volatile uint32_t overflowCount_ = 0;
};
