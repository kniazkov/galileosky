/**
 * @file sensor_events.cpp
 * @brief Формирует серверные сообщения по фронтам дискретных датчиков.
 *
 * Первый достоверный уровень используется как начальная точка. Сообщение
 * создаётся только после наблюдаемого перехода из нуля в единицу.
 */

#include "modules/sensor_events.hpp"

#include "drivers/server_transport.hpp"
#include "modules/server_transmission.hpp"

namespace {

constexpr std::uint8_t ignition_event = 1U;
constexpr std::uint8_t door_open_event = 2U;
constexpr std::uint8_t alarm_event = 3U;

/**
 * @brief Хранит наблюдавшиеся входы, их уровни и следующий номер сообщения.
 */
struct EventState {
    std::uint8_t observed_mask{};
    std::uint8_t active_mask{};
    std::uint32_t next_message_id{1U};
};

EventState state{};

drivers::server_transport::Message make_message(
    const std::uint8_t event_code,
    const std::uint32_t timestamp_ms)
{
    drivers::server_transport::Message message{};
    message.message_id = state.next_message_id;
    message.length = 5U;
    message.payload[0U] = event_code;
    message.payload[1U] = static_cast<std::uint8_t>(timestamp_ms);
    message.payload[2U] = static_cast<std::uint8_t>(timestamp_ms >> 8U);
    message.payload[3U] = static_cast<std::uint8_t>(timestamp_ms >> 16U);
    message.payload[4U] = static_cast<std::uint8_t>(timestamp_ms >> 24U);
    return message;
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
        const drivers::server_transport::Message message =
            make_message(event_code, timestamp_ms);
        if (modules::server_transmission::enqueue(message)) {
            ++state.next_message_id;
        }
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
    state.next_message_id = 1U;
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
