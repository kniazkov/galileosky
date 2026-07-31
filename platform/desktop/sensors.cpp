/**
 * @file sensors.cpp
 * @brief Реализует управляемые датчики desktop-стенда.
 *
 * Канал становится достоверным только после явной установки значения
 * сценарием.
 */

#include "drivers/sensors.hpp"
#include "platform/desktop/sensors_adapter.hpp"

#include <array>
#include <cstddef>

namespace {

constexpr std::size_t analog_channel_count = 2U;
constexpr std::size_t digital_input_count = 3U;

std::array<std::uint16_t, analog_channel_count> analog_values{};
std::array<bool, analog_channel_count> analog_valid{};
std::array<bool, digital_input_count> digital_values{};
std::array<bool, digital_input_count> digital_valid{};

std::size_t index_of(const drivers::sensors::AnalogChannel channel)
{
    return static_cast<std::size_t>(channel);
}

std::size_t index_of(const drivers::sensors::DigitalInput input)
{
    return static_cast<std::size_t>(input);
}

}  // безымянное пространство имён

namespace drivers::sensors {

void reset()
{
    analog_values = {};
    analog_valid = {};
    digital_values = {};
    digital_valid = {};
}

bool read_analog(const AnalogChannel channel, std::uint16_t& value)
{
    const std::size_t index = index_of(channel);
    if (!analog_valid[index]) {
        return false;
    }
    value = analog_values[index];
    return true;
}

bool read_digital(const DigitalInput input, bool& active)
{
    const std::size_t index = index_of(input);
    if (!digital_valid[index]) {
        return false;
    }
    active = digital_values[index];
    return true;
}

}  // пространство имён drivers::sensors

namespace platform::desktop_sensors {

bool set_analog(
    const drivers::sensors::AnalogChannel channel,
    const std::uint16_t value)
{
    if (value > drivers::sensors::maximum_adc_value) {
        return false;
    }

    const std::size_t index = index_of(channel);
    analog_values[index] = value;
    analog_valid[index] = true;
    return true;
}

void set_digital(
    const drivers::sensors::DigitalInput input,
    const bool active)
{
    const std::size_t index = index_of(input);
    digital_values[index] = active;
    digital_valid[index] = true;
}

}  // пространство имён platform::desktop_sensors
