#include <gtest/gtest.h>

#include <string>

#include "chords.h"
#include "sequence.h"
#include "storage_stub.h"


namespace {

ResolvedChord chord(int root, ChordType type, bool add9 = false,
                    bool add11 = false, bool add13 = false) {
    ResolvedChord c;
    c.rootPc = root;
    c.type = type;
    c.add9 = add9;
    c.add11 = add11;
    c.add13 = add13;
    return c;
}

} // namespace


TEST(Sequence, ChordIdRoundTrips) {
    for (int root = 0; root < 12; root++) {
        for (int type = 0; type < static_cast<int>(ChordType::COUNT); type++) {
            for (int ext = 0; ext < 8; ext++) {
                ResolvedChord c = chord(root, static_cast<ChordType>(type),
                                        ext & 1, ext & 2, ext & 4);
                uint16_t id = sequenceChordId(c);
                ResolvedChord out;
                ASSERT_TRUE(sequenceChordFromId(id, out)) << id;
                EXPECT_EQ(out.rootPc, root);
                EXPECT_EQ(out.type, c.type);
                EXPECT_EQ(out.add9, c.add9);
                EXPECT_EQ(out.add11, c.add11);
                EXPECT_EQ(out.add13, c.add13);
            }
        }
    }
}

TEST(Sequence, ChordIdRejectsInvalid) {
    ResolvedChord out;
    EXPECT_FALSE(sequenceChordFromId(12 * 128, out));   // root 12
    EXPECT_FALSE(sequenceChordFromId(11 * 128 + 11 * 8, out));  // type 11
}

TEST(Sequence, SetEventSortsReplacesAndCaps) {
    Sequence s;
    s.setEvent(32, chord(5, ChordType::Minor));
    s.setEvent(0, chord(0, ChordType::Major));
    s.setEvent(16, chord(7, ChordType::Dom7));
    s.setEvent(16, chord(9, ChordType::Min7));  // replace

    ASSERT_EQ(s.events.size(), 3u);
    EXPECT_EQ(s.events[0].step, 0);
    EXPECT_EQ(s.events[1].step, 16);
    EXPECT_EQ(s.events[1].chord.rootPc, 9);
    EXPECT_EQ(s.events[2].step, 32);
    EXPECT_EQ(s.eventAt(16)->chord.type, ChordType::Min7);
    EXPECT_EQ(s.eventAt(1), nullptr);

    for (int i = 0; i < MAX_SEQUENCE_EVENTS; i++) {
        s.setEvent(static_cast<uint16_t>(100 + i), chord(0, ChordType::Major));
    }
    EXPECT_LE(static_cast<int>(s.events.size()), MAX_SEQUENCE_EVENTS);
    EXPECT_FALSE(s.setEvent(600, chord(0, ChordType::Major)));
}

TEST(Sequence, MergedParamsUsesRecordedAndLiveBits) {
    Sequence s;
    s.recorded.octave = 2;
    s.recorded.velocity = 42;
    s.recorded.voicing_mode = VoicingMode::Smart;
    s.recorded.play_mode = PlayMode::Arpeggio;
    s.recorded.arp_rate = ArpRate::Eighth;
    s.recorded.channel = 9;
    s.live_mask = static_cast<uint16_t>(1u <<
        static_cast<int>(SequenceSetting::Velocity));

    ChordParams live = ChordParams::defaults();
    live.octave = -1;
    live.velocity = 111;
    live.play_mode = PlayMode::Pattern;
    live.arp_rate = ArpRate::ThirtySecond;
    live.channel = 3;

    ChordParams out = s.mergedParams(live, true);
    EXPECT_EQ(out.octave, 2);                    // recorded
    EXPECT_EQ(out.velocity, 111);                // live bit set
    EXPECT_EQ(out.voicing_mode, VoicingMode::Smart);
    EXPECT_EQ(out.play_mode, PlayMode::Arpeggio); // recorded
    EXPECT_EQ(out.arp_rate, ArpRate::Eighth);     // recorded
    EXPECT_EQ(out.channel, 3);                   // always live

    s.live_mask |= static_cast<uint16_t>(
        1u << static_cast<int>(SequenceSetting::PlayMode));
    ChordParams liveMode = s.mergedParams(live, true);
    EXPECT_EQ(liveMode.play_mode, PlayMode::Pattern);  // live bit set
    EXPECT_EQ(liveMode.arp_rate, ArpRate::Eighth);     // still recorded

    ChordParams allLive = s.mergedParams(live, false);
    EXPECT_EQ(allLive.octave, -1);
    EXPECT_EQ(allLive.velocity, 111);
    EXPECT_EQ(allLive.voicing_mode, live.voicing_mode);
    EXPECT_EQ(allLive.play_mode, PlayMode::Pattern);
    EXPECT_EQ(allLive.arp_rate, ArpRate::ThirtySecond);
}

TEST(Sequence, ParseSerializeRoundTrip) {
    SequencerLibrary lib;
    lib.slots[1].bars = 8;
    lib.slots[1].steps_per_bar = 12;
    lib.slots[1].recorded.octave = -2;
    lib.slots[1].recorded.velocity = 90;
    lib.slots[1].recorded.pattern = 3;
    lib.slots[1].recorded.play_mode = PlayMode::ArpHold;
    lib.slots[1].recorded.arp_rate = ArpRate::Eighth;
    lib.slots[1].live_mask = static_cast<uint16_t>(
        (1u << static_cast<int>(SequenceSetting::Pan)) |
        (1u << static_cast<int>(SequenceSetting::Pattern)) |
        (1u << static_cast<int>(SequenceSetting::PlayMode)));
    lib.slots[1].setEvent(0, chord(0, ChordType::Major));
    lib.slots[1].setEvent(5, chord(9, ChordType::Min7, true, false, true));

    std::string json = serializeSequenceFile(lib);
    EXPECT_NE(json.find("\"bars\":8"), std::string::npos);
    EXPECT_NE(json.find("\"steps_per_bar\":12"), std::string::npos);

    SequencerLibrary out;
    ASSERT_TRUE(parseSequenceFile(json, out));
    ASSERT_EQ(out.slots[1].events.size(), 2u);
    EXPECT_EQ(out.slots[1].bars, 8);
    EXPECT_EQ(out.slots[1].steps_per_bar, 12);
    EXPECT_EQ(out.slots[1].recorded.octave, -2);
    EXPECT_EQ(out.slots[1].recorded.velocity, 90);
    EXPECT_EQ(out.slots[1].recorded.pattern, 3);
    EXPECT_EQ(out.slots[1].recorded.play_mode, PlayMode::ArpHold);
    EXPECT_EQ(out.slots[1].recorded.arp_rate, ArpRate::Eighth);
    EXPECT_EQ(out.slots[1].live_mask, lib.slots[1].live_mask);
    EXPECT_EQ(out.slots[1].events[0].chord.type, ChordType::Major);
    EXPECT_EQ(out.slots[1].events[1].chord.rootPc, 9);
    EXPECT_TRUE(out.slots[1].events[1].chord.add9);
    EXPECT_TRUE(out.slots[1].events[1].chord.add13);
    EXPECT_TRUE(out.slots[0].isDefault());
}

TEST(Sequence, ParseLegacyRecordedArrayDefaultsNewSettings) {
    std::string json = R"({
      "sequences": [
        {"bars":1,"steps_per_bar":16,
         "recorded":[0,500,100,64,0,0,3,0,0,0,0],
         "events":[[0,0]]}
      ]
    })";

    SequencerLibrary lib;
    ASSERT_TRUE(parseSequenceFile(json, lib));
    EXPECT_EQ(lib.slots[0].recorded.play_mode, PlayMode::Held);
    EXPECT_EQ(lib.slots[0].recorded.arp_rate, ArpRate::Sixteenth);
    ASSERT_EQ(lib.slots[0].events.size(), 1u);
}

TEST(Sequence, ParseSkipsMalformedAndOutOfRangeEvents) {
    std::string json = R"({
      "sequences": [
        {"bars":4,"steps_per_bar":16,"events":[
          [0, 0],
          [999, 0],
          [4, 99999],
          [8, 65535],
          {"step":12,"chord":256},
          [16]
        ]}
      ]
    })";

    SequencerLibrary lib;
    ASSERT_TRUE(parseSequenceFile(json, lib));
    ASSERT_EQ(lib.slots[0].events.size(), 2u);
    EXPECT_EQ(lib.slots[0].events[0].step, 0);
    EXPECT_EQ(lib.slots[0].events[1].step, 12);
}

TEST(Sequence, ParseAcceptsLegacyBareArray) {
    std::string json = R"([{"bars":1,"steps_per_bar":4,"events":[[0,0]]}])";
    SequencerLibrary lib;
    ASSERT_TRUE(parseSequenceFile(json, lib));
    EXPECT_EQ(lib.slots[0].bars, 1);
    ASSERT_EQ(lib.slots[0].events.size(), 1u);
}

TEST(Sequence, ParseRejectsGarbage) {
    SequencerLibrary lib;
    EXPECT_FALSE(parseSequenceFile("not json", lib));
    EXPECT_FALSE(parseSequenceFile("{}", lib));
    EXPECT_FALSE(parseSequenceFile("[1,2,3]", lib));
}

TEST(Sequence, StorageSaveLoadRemove) {
    StorageStub storage;
    SequencerLibrary lib;
    lib.slots[0].bars = 2;
    lib.slots[0].setEvent(0, chord(2, ChordType::Minor));
    lib.slots[0].setEvent(8, chord(7, ChordType::Maj7));

    EXPECT_TRUE(saveSequenceFile(storage, 0, 0, lib));
    EXPECT_TRUE(storage.exists("/sequences/b1p1.json"));

    SequencerLibrary loaded = loadSequenceFile(storage, 0, 0);
    ASSERT_EQ(loaded.slots[0].events.size(), 2u);
    EXPECT_EQ(loaded.slots[0].bars, 2);
    EXPECT_EQ(loaded.slots[0].events[1].chord.type, ChordType::Maj7);

    EXPECT_TRUE(removeSequenceFile(storage, 0, 0));
    EXPECT_FALSE(storage.exists("/sequences/b1p1.json"));
    EXPECT_TRUE(loadSequenceFile(storage, 0, 0).slots[0].isDefault());
}

TEST(Sequence, EmptyLibraryRemovesFile) {
    StorageStub storage;
    SequencerLibrary lib;
    lib.slots[3].bars = 1;
    EXPECT_TRUE(saveSequenceFile(storage, 2, 4, lib));
    EXPECT_TRUE(storage.exists("/sequences/b3p5.json"));

    SequencerLibrary empty;
    EXPECT_TRUE(saveSequenceFile(storage, 2, 4, empty));
    EXPECT_FALSE(storage.exists("/sequences/b3p5.json"));
}

TEST(Sequence, WorstCaseFileFitsBudget) {
    SequencerLibrary lib;
    for (auto& seq : lib.slots) {
        seq.bars = 8;
        seq.steps_per_bar = 16;
        seq.recorded.octave = 1;
        seq.recorded.velocity = 101;
        seq.live_mask = 0x5A5;
        for (int i = 0; i < MAX_SEQUENCE_EVENTS; i++) {
            seq.setEvent(static_cast<uint16_t>(i * 2),
                         chord(i % 12, static_cast<ChordType>(i % 11),
                               i & 1, i & 2, i & 4));
        }
    }
    std::string json = serializeSequenceFile(lib);
    // The 40-preset budget assumes ~7 KB per fully-populated file.
    EXPECT_LT(json.size(), 7000u) << json.size();
}

TEST(Sequence, ClampBars) {
    EXPECT_EQ(clampSequenceBars(-1), 1);
    EXPECT_EQ(clampSequenceBars(1), 1);
    EXPECT_EQ(clampSequenceBars(2), 2);
    EXPECT_EQ(clampSequenceBars(3), 4);
    EXPECT_EQ(clampSequenceBars(4), 4);
    EXPECT_EQ(clampSequenceBars(5), 8);
    EXPECT_EQ(clampSequenceBars(8), 8);
}
