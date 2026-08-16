#include <gtest/gtest.h>
#include "config.h"
#include "storage_stub.h"
#include "params.h"


class ConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
        storage_.clear();
    }
    StorageStub storage_;
};

TEST_F(ConfigTest, Defaults) {
    AppConfig cfg = AppConfig::defaults();
    EXPECT_TRUE(cfg.din_enabled);
    EXPECT_FALSE(cfg.midi_clock_enabled);
    EXPECT_EQ(cfg.base_root_midi, 60);
    EXPECT_EQ(cfg.note_range_low, 48);
    EXPECT_EQ(cfg.note_range_high, 84);
    EXPECT_EQ(cfg.startup_preset, "B1:P1");
}

TEST_F(ConfigTest, LoadWhenMissingReturnsDefaults) {
    AppConfig cfg = AppConfig::load(storage_);
    EXPECT_TRUE(cfg.din_enabled);
    EXPECT_EQ(cfg.base_root_midi, 60);
}

TEST_F(ConfigTest, LoadEmptyConfigReturnsDefaults) {
    storage_.writeFile("/config.json", "");
    AppConfig cfg = AppConfig::load(storage_);
    EXPECT_TRUE(cfg.din_enabled);
}

TEST_F(ConfigTest, LoadInvalidJsonReturnsDefaults) {
    storage_.writeFile("/config.json", "{not valid json}");
    AppConfig cfg = AppConfig::load(storage_);
    EXPECT_TRUE(cfg.din_enabled);  // falls back to default
}

TEST_F(ConfigTest, LoadMinimalValidConfig) {
    const char* json = R"({"midi":{"din_enabled":false,"clock_enabled":true}})";
    storage_.writeFile("/config.json", json);
    AppConfig cfg = AppConfig::load(storage_);
    EXPECT_FALSE(cfg.din_enabled);
    EXPECT_TRUE(cfg.midi_clock_enabled);
    // Other fields use defaults
    EXPECT_EQ(cfg.base_root_midi, 60);
}

TEST_F(ConfigTest, LoadClampsOutOfRange) {
    const char* json = R"({"chord":{"base_root_midi":999,"note_range":[-10,999]}})";
    storage_.writeFile("/config.json", json);
    AppConfig cfg = AppConfig::load(storage_);
    EXPECT_EQ(cfg.base_root_midi, 60);     // clamped to default (64 is within range 0-127? No, 999 -> clamped to 127? Wait, the clamp uses fallback. Let me check the code...)
    // Actually clamping function uses the fallback value when out of range. So 999 > 127 falls back to default (60).
    // This is correct behavior per NFR-9: out-of-range values fall back to defaults.
}

TEST_F(ConfigTest, SaveAndLoadRoundTrip) {
    AppConfig cfg = AppConfig::defaults();
    cfg.din_enabled = false;
    cfg.base_root_midi = 72;
    cfg.note_range_low = 36;
    cfg.note_range_high = 96;
    cfg.midi_clock_enabled = true;

    EXPECT_TRUE(cfg.save(storage_));
    EXPECT_TRUE(storage_.exists("/config.json"));

    AppConfig loaded = AppConfig::load(storage_);
    EXPECT_EQ(loaded.din_enabled, cfg.din_enabled);
    EXPECT_EQ(loaded.base_root_midi, cfg.base_root_midi);
    EXPECT_EQ(loaded.note_range_low, cfg.note_range_low);
    EXPECT_EQ(loaded.note_range_high, cfg.note_range_high);
    EXPECT_EQ(loaded.midi_clock_enabled, cfg.midi_clock_enabled);
}

TEST_F(ConfigTest, LoadWithLedSettings) {
    const char* json = R"({"led":{"bpm_indicator":false,"flash_ms":50}})";
    storage_.writeFile("/config.json", json);
    AppConfig cfg = AppConfig::load(storage_);
    EXPECT_FALSE(cfg.bpm_indicator);
    EXPECT_EQ(cfg.led_flash_ms, 50);
}

TEST_F(ConfigTest, DefaultsForNewFields) {
    AppConfig cfg = AppConfig::defaults();
    EXPECT_EQ(cfg.cursor_timeout_ms, 5000);
    EXPECT_EQ(cfg.menu_timeout_ms, 10000);
    EXPECT_EQ(cfg.led_indicator, LedTarget::NumLock);
}

TEST_F(ConfigTest, LoadLedTargetChoice) {
    const char* json = R"({"led":{"led":"num_lock"}})";
    storage_.writeFile("/config.json", json);
    AppConfig cfg = AppConfig::load(storage_);
    EXPECT_EQ(cfg.led_indicator, LedTarget::NumLock);

    storage_.writeFile("/config.json", R"({"led":{"led":"caps_lock"}})");
    cfg = AppConfig::load(storage_);
    EXPECT_EQ(cfg.led_indicator, LedTarget::CapsLock);

    storage_.writeFile("/config.json", R"({"led":{"led":"scroll_lock"}})");
    cfg = AppConfig::load(storage_);
    EXPECT_EQ(cfg.led_indicator, LedTarget::ScrollLock);

    storage_.writeFile("/config.json", R"({"led":{"led":"all"}})");
    cfg = AppConfig::load(storage_);
    EXPECT_EQ(cfg.led_indicator, LedTarget::All);

    // Unknown value falls back to num lock.
    storage_.writeFile("/config.json", R"({"led":{"led":"bogus"}})");
    cfg = AppConfig::load(storage_);
    EXPECT_EQ(cfg.led_indicator, LedTarget::NumLock);
}

TEST_F(ConfigTest, LoadMenuTimeout) {
    const char* json = R"({"display":{"menu_timeout_ms":7000}})";
    storage_.writeFile("/config.json", json);
    AppConfig cfg = AppConfig::load(storage_);
    EXPECT_EQ(cfg.menu_timeout_ms, 7000);
}

TEST_F(ConfigTest, LedTargetRoundTrip) {
    AppConfig cfg = AppConfig::defaults();
    cfg.led_indicator = LedTarget::NumLock;
    EXPECT_TRUE(cfg.save(storage_));

    AppConfig loaded = AppConfig::load(storage_);
    EXPECT_EQ(loaded.led_indicator, LedTarget::NumLock);

    cfg.led_indicator = LedTarget::All;
    EXPECT_TRUE(cfg.save(storage_));
    loaded = AppConfig::load(storage_);
    EXPECT_EQ(loaded.led_indicator, LedTarget::All);
}
