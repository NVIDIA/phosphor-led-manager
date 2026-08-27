#include "config.hpp"

#include <string>

#include <gtest/gtest.h>

using namespace phosphor::led::gpio;

// ---------------------------------------------------------------------------
// Config parsing
// ---------------------------------------------------------------------------

TEST(GpioLedConfigParse, ValidConfigParses)
{
    const std::string raw = R"({
        "leds": [
            {
                "name": "uid_led",
                "output_gpio": "UID_OUT",
                "input_gpio": "UID_IN",
                "output_active_low": true, "input_active_low": true
            },
            {
                "name": "fault_led",
                "output_gpio": "FAULT_OUT",
                "output_active_low": false,
                "input_gpio": "FAULT_IN",
                "input_active_low": true
            }
        ]
    })";

    GpioLedConfig cfg = parseAndValidate(raw);
    ASSERT_EQ(cfg.leds.size(), 2U);
    EXPECT_EQ(cfg.leds[0].name, "uid_led");
    EXPECT_EQ(cfg.leds[0].outputGpio, "UID_OUT");
    EXPECT_EQ(cfg.leds[0].inputGpio, "UID_IN");
    EXPECT_TRUE(cfg.leds[0].outputActiveLow);
    EXPECT_TRUE(cfg.leds[0].inputActiveLow);

    // The two lines carry independent polarity.
    EXPECT_EQ(cfg.leds[1].name, "fault_led");
    EXPECT_FALSE(cfg.leds[1].outputActiveLow);
    EXPECT_TRUE(cfg.leds[1].inputActiveLow);
}

TEST(GpioLedConfigParse, MissingOutputActiveLowThrows)
{
    // Polarity must be stated per line, not assumed.
    const std::string raw =
        R"({"leds":[{"name":"x","output_gpio":"O","input_gpio":"I","input_active_low":true}]})";
    EXPECT_THROW(parseAndValidate(raw), ConfigError);
}

TEST(GpioLedConfigParse, MissingInputActiveLowThrows)
{
    const std::string raw =
        R"({"leds":[{"name":"x","output_gpio":"O","input_gpio":"I","output_active_low":true}]})";
    EXPECT_THROW(parseAndValidate(raw), ConfigError);
}

TEST(GpioLedConfigParse, NonBooleanActiveLowThrows)
{
    const std::string raw =
        R"({"leds":[{"name":"x","output_gpio":"O","input_gpio":"I","output_active_low":"true","input_active_low":true}]})";
    EXPECT_THROW(parseAndValidate(raw), ConfigError);
}

TEST(GpioLedConfigParse, MalformedJsonThrows)
{
    EXPECT_THROW(parseAndValidate("{ this is not json"), ConfigError);
}

TEST(GpioLedConfigParse, MissingRequiredFieldThrows)
{
    const std::string raw =
        R"({"leds":[{"name":"x","input_gpio":"I","output_active_low":true,"input_active_low":true}]})";
    EXPECT_THROW(parseAndValidate(raw), ConfigError);
}

TEST(GpioLedConfigParse, EmptyNameThrows)
{
    const std::string raw =
        R"({"leds":[{"name":"","output_gpio":"O","input_gpio":"I","output_active_low":true,"input_active_low":true}]})";
    EXPECT_THROW(parseAndValidate(raw), ConfigError);
}

TEST(GpioLedConfigParse, InvalidNameCharacterThrows)
{
    // The name becomes a D-Bus object path element: only [A-Za-z0-9_] allowed.
    const std::string raw =
        R"({"leds":[{"name":"uid-led","output_gpio":"O","input_gpio":"I","output_active_low":true,"input_active_low":true}]})";
    EXPECT_THROW(parseAndValidate(raw), ConfigError);
}

TEST(GpioLedConfigParse, LedsNotArrayThrows)
{
    EXPECT_THROW(parseAndValidate(R"({"leds":{}})"), ConfigError);
}

TEST(GpioLedConfigParse, MissingLedsThrows)
{
    EXPECT_THROW(parseAndValidate(R"({})"), ConfigError);
}

TEST(GpioLedConfigParse, EmptyLedsArrayThrows)
{
    EXPECT_THROW(parseAndValidate(R"({"leds":[]})"), ConfigError);
}

TEST(GpioLedConfigParse, DuplicateLedNameThrows)
{
    const std::string raw = R"({
        "leds": [
            {"name": "dup", "output_gpio": "O1", "input_gpio": "I1", "output_active_low": true, "input_active_low": true},
            {"name": "dup", "output_gpio": "O2", "input_gpio": "I2", "output_active_low": true, "input_active_low": true}
        ]
    })";
    EXPECT_THROW(parseAndValidate(raw), ConfigError);
}

TEST(GpioLedConfigParse, DuplicateGpioLineThrows)
{
    const std::string raw = R"({
        "leds": [
            {"name": "a", "output_gpio": "SHARED", "input_gpio": "I1", "output_active_low": true, "input_active_low": true},
            {"name": "b", "output_gpio": "SHARED", "input_gpio": "I2", "output_active_low": true, "input_active_low": true}
        ]
    })";
    EXPECT_THROW(parseAndValidate(raw), ConfigError);
}

TEST(GpioLedConfigParse, OutputEqualsInputThrows)
{
    const std::string raw =
        R"({"leds":[{"name":"x","output_gpio":"S","input_gpio":"S","output_active_low":true,"input_active_low":true}]})";
    EXPECT_THROW(parseAndValidate(raw), ConfigError);
}
