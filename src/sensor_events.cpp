/**
 * @file sensor_events.cpp
 * @brief Формирует серверные сообщения по фронтам дискретных датчиков.
 *
 * Первый достоверный уровень используется как начальная точка. Сообщение
 * создаётся только после наблюдаемого перехода из нуля в единицу.
 */

#include "modules/sensor_events.hpp"

#include "modules/event_messages.hpp"

#include <array>

namespace {

constexpr std::uint8_t ignition_event = 1U;
constexpr std::uint8_t door_open_event = 2U;
constexpr std::uint8_t alarm_event = 3U;

/**
 * @brief Хранит наблюдавшиеся входы и их последние уровни.
 */
struct EventState {
    std::uint8_t observed_mask{};
    std::uint8_t active_mask{};
};

EventState state{};

void send_event(
    const std::uint8_t event_code,
    const std::uint32_t timestamp_ms)
{
    std::array<std::uint8_t, 5U> payload{};
    payload[0U] = event_code;
    payload[1U] = static_cast<std::uint8_t>(timestamp_ms);
    payload[2U] = static_cast<std::uint8_t>(timestamp_ms >> 8U);
    payload[3U] = static_cast<std::uint8_t>(timestamp_ms >> 16U);
    payload[4U] = static_cast<std::uint8_t>(timestamp_ms >> 24U);
    modules::event_messages::enqueue(
        payload.data(),
        static_cast<std::uint8_t>(payload.size()));
}

void process_input(
    const bool active,
    const std::uint8_t validity_bit,
    const std::uint8_t event_code,
    const std::uint32_t timestamp_ms)
{
    if ((state.observed_mask & validity_bit) == 0U) {
        state.observed_mask |= validity_bit;
        if (active) {
            state.active_mask |= validity_bit;
        }
        return;
    }

    const bool was_active = (state.active_mask & validity_bit) != 0U;
    if (active && !was_active) {
        send_event(event_code, timestamp_ms);
    }

    if (active) {
        state.active_mask |= validity_bit;
    } else {
        state.active_mask &= static_cast<std::uint8_t>(~validity_bit);
    }
}

}  // безымянное пространство имён

namespace modules::sensor_events {

void reset()
{
    state = {};
}

void tick(
    const std::uint32_t timestamp_ms,
    const modules::sensors::Snapshot& sensors)
{
    using namespace modules::sensors;

    if ((sensors.valid_mask & ignition_valid) != 0U) {
        process_input(
            sensors.ignition,
            ignition_valid,
            ignition_event,
            timestamp_ms);
    }
    if ((sensors.valid_mask & door_open_valid) != 0U) {
        process_input(
            sensors.door_open,
            door_open_valid,
            door_open_event,
            timestamp_ms);
    }
    if ((sensors.valid_mask & alarm_valid) != 0U) {
        process_input(
            sensors.alarm,
            alarm_valid,
            alarm_event,
            timestamp_ms);
    }
}

}  // пространство имён modules::sensor_events
