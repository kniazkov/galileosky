/**
 * @file vehicle.cpp
 * @brief Реализует статический модуль параметров автомобиля.
 *
 * Входящие кадры обрабатываются до опустошения очереди, а пять запросов OBD-II
 * отправляются раз в секунду без немедленного повтора при недоступном драйвере.
 */

#include "modules/vehicle.hpp"

#include "drivers/can.hpp"
#include "protocol/obd2.hpp"

#include <array>

namespace {

constexpr std::uint32_t poll_interval_ms = 1'000U;
constexpr std::array<protocol::obd2::Parameter, 5U> parameters{
    protocol::obd2::Parameter::engine_speed,
    protocol::obd2::Parameter::vehicle_speed,
    protocol::obd2::Parameter::coolant_temperature,
    protocol::obd2::Parameter::fuel_level,
    protocol::obd2::Parameter::check_engine
};

/**
 * @brief Хранит параметры и состояние периодического опроса.
 */
struct VehicleState {
    modules::vehicle::Snapshot snapshot{};
    std::uint32_t last_poll_timestamp_ms{};
    bool polling_started{};
};

VehicleState state{};

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

void apply(
    const protocol::obd2::DecodedParameter& decoded,
    const std::uint32_t timestamp_ms)
{
    using protocol::obd2::Parameter;
    using namespace modules::vehicle;

    bool changed = false;
    switch (decoded.parameter) {
    case Parameter::engine_speed:
        state.snapshot.engine_speed_timestamp_ms = timestamp_ms;
        changed = update_value(
            state.snapshot.engine_speed_rpm,
            static_cast<std::uint16_t>(decoded.value),
            engine_speed_valid);
        break;
    case Parameter::vehicle_speed:
        changed = update_value(
            state.snapshot.vehicle_speed_kmh,
            static_cast<std::uint8_t>(decoded.value),
            vehicle_speed_valid);
        break;
    case Parameter::coolant_temperature:
        changed = update_value(
            state.snapshot.coolant_temperature_c,
            static_cast<std::int16_t>(decoded.value),
            coolant_temperature_valid);
        break;
    case Parameter::fuel_level:
        changed = update_value(
            state.snapshot.fuel_level_percent,
            static_cast<std::uint8_t>(decoded.value),
            fuel_level_valid);
        break;
    case Parameter::check_engine:
        changed = update_value(
            state.snapshot.check_engine,
            decoded.value != 0,
            check_engine_valid);
        break;
    }

    if (changed) {
        ++state.snapshot.revision;
    }
}

void process_received_frames(const std::uint32_t timestamp_ms)
{
    drivers::can::Frame frame{};
    while (drivers::can::receive(frame)) {
        protocol::obd2::DecodedParameter decoded{};
        if (protocol::obd2::decode(frame, decoded)) {
            apply(decoded, timestamp_ms);
        }
    }
}

void poll_if_due(const std::uint32_t timestamp_ms)
{
    if (!state.polling_started) {
        state.last_poll_timestamp_ms = timestamp_ms;
        state.polling_started = true;
        return;
    }

    if (timestamp_ms - state.last_poll_timestamp_ms < poll_interval_ms) {
        return;
    }

    state.last_poll_timestamp_ms = timestamp_ms;
    for (const protocol::obd2::Parameter parameter : parameters) {
        const drivers::can::Frame frame = protocol::obd2::make_request(parameter);
        (void)drivers::can::send(frame);
    }
}

}  // безымянное пространство имён

namespace modules::vehicle {

void reset()
{
    state = {};
}

void tick(const std::uint32_t timestamp_ms)
{
    process_received_frames(timestamp_ms);
    poll_if_due(timestamp_ms);
}

Snapshot snapshot()
{
    return state.snapshot;
}

}  // пространство имён modules::vehicle
