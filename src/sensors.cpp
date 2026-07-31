/**
 * @file sensors.cpp
 * @brief Реализует периодический опрос аналоговых и дискретных датчиков.
 */

#include "modules/sensors.hpp"

#include "drivers/sensors.hpp"

namespace {

constexpr std::uint32_t polling_interval_ms = 100U;

/**
 * @brief Хранит значения датчиков и состояние расписания опроса.
 */
struct SensorState {
    modules::sensors::Snapshot snapshot{};
    std::uint32_t last_poll_timestamp_ms{};
    bool polling_started{};
};

SensorState state{};

template<typename Value>
bool update_value(
    Value& target,
    const Value value,
    const std::uint8_t validity_bit)
{
    const bool changed =
        (state.snapshot.valid_mask & validity_bit) == 0U || target != value;
    target = value;
    state.snapshot.valid_mask |= validity_bit;
    return changed;
}

void poll()
{
    using namespace modules::sensors;

    bool changed = false;
    std::uint16_t analog_value = 0U;
    if (drivers::sensors::read_analog(
            drivers::sensors::AnalogChannel::supply_voltage,
            analog_value)) {
        changed = update_value(
            state.snapshot.supply_voltage_adc,
            analog_value,
            supply_voltage_valid) || changed;
    }
    if (drivers::sensors::read_analog(
            drivers::sensors::AnalogChannel::external_input,
            analog_value)) {
        changed = update_value(
            state.snapshot.external_input_adc,
            analog_value,
            external_input_valid) || changed;
    }

    bool digital_value = false;
    if (drivers::sensors::read_digital(
            drivers::sensors::DigitalInput::ignition,
            digital_value)) {
        changed = update_value(
            state.snapshot.ignition,
            digital_value,
            ignition_valid) || changed;
    }
    if (drivers::sensors::read_digital(
            drivers::sensors::DigitalInput::door_open,
            digital_value)) {
        changed = update_value(
            state.snapshot.door_open,
            digital_value,
            door_open_valid) || changed;
    }
    if (drivers::sensors::read_digital(
            drivers::sensors::DigitalInput::alarm,
            digital_value)) {
        changed = update_value(
            state.snapshot.alarm,
            digital_value,
            alarm_valid) || changed;
    }

    if (changed) {
        ++state.snapshot.revision;
    }
}

}  // безымянное пространство имён

namespace modules::sensors {

void reset()
{
    state = {};
}

void tick(const std::uint32_t timestamp_ms)
{
    if (state.polling_started
        && timestamp_ms - state.last_poll_timestamp_ms < polling_interval_ms) {
        return;
    }

    state.polling_started = true;
    state.last_poll_timestamp_ms = timestamp_ms;
    poll();
}

Snapshot snapshot()
{
    return state.snapshot;
}

}  // пространство имён modules::sensors
