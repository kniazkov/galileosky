/**
 * @file obd2.cpp
 * @brief Формирует запросы и декодирует поддерживаемые ответы OBD-II.
 */

#include "protocol/obd2.hpp"

#include <cstddef>

namespace {

constexpr std::uint16_t functional_request_identifier = 0x07DFU;
constexpr std::uint16_t first_response_identifier = 0x07E8U;
constexpr std::uint16_t last_response_identifier = 0x07EFU;
constexpr std::uint8_t current_data_request_mode = 0x01U;
constexpr std::uint8_t current_data_response_mode = 0x41U;

constexpr std::uint8_t monitor_status_pid = 0x01U;
constexpr std::uint8_t coolant_temperature_pid = 0x05U;
constexpr std::uint8_t engine_speed_pid = 0x0CU;
constexpr std::uint8_t vehicle_speed_pid = 0x0DU;
constexpr std::uint8_t fuel_level_pid = 0x2FU;

std::uint8_t fuel_percentage(const std::uint8_t raw_value)
{
    const std::uint16_t dividend =
        static_cast<std::uint16_t>(raw_value) * 100U + 127U;
    const std::uint16_t adjusted = dividend + 1U;
    return static_cast<std::uint8_t>(
        (adjusted + (adjusted >> 8U)) >> 8U);
}

std::uint8_t pid_for(const protocol::obd2::Parameter parameter)
{
    using protocol::obd2::Parameter;

    switch (parameter) {
    case Parameter::engine_speed:
        return engine_speed_pid;
    case Parameter::vehicle_speed:
        return vehicle_speed_pid;
    case Parameter::coolant_temperature:
        return coolant_temperature_pid;
    case Parameter::fuel_level:
        return fuel_level_pid;
    case Parameter::check_engine:
        return monitor_status_pid;
    }

    return 0U;
}

bool decode_value(
    const std::uint8_t pid,
    const drivers::can::Frame& frame,
    protocol::obd2::DecodedParameter& decoded)
{
    using protocol::obd2::Parameter;

    const std::uint8_t first = frame.data[3U];
    switch (pid) {
    case engine_speed_pid:
        if (frame.data[0U] < 4U) {
            return false;
        }
        decoded = {
            Parameter::engine_speed,
            static_cast<std::int32_t>(
                (static_cast<std::uint16_t>(first) << 8U)
                | frame.data[4U]) / 4};
        return true;
    case vehicle_speed_pid:
        decoded = {Parameter::vehicle_speed, first};
        return true;
    case coolant_temperature_pid:
        decoded = {
            Parameter::coolant_temperature,
            static_cast<std::int32_t>(first) - 40};
        return true;
    case fuel_level_pid:
        decoded = {
            Parameter::fuel_level,
            fuel_percentage(first)};
        return true;
    case monitor_status_pid:
        decoded = {Parameter::check_engine, (first & 0x80U) != 0U};
        return true;
    default:
        return false;
    }
}

}  // безымянное пространство имён

namespace protocol::obd2 {

drivers::can::Frame make_request(const Parameter parameter)
{
    drivers::can::Frame frame{};
    frame.identifier = functional_request_identifier;
    frame.length = drivers::can::maximum_data_length;
    frame.data[0U] = 2U;
    frame.data[1U] = current_data_request_mode;
    frame.data[2U] = pid_for(parameter);
    return frame;
}

bool decode(
    const drivers::can::Frame& frame,
    DecodedParameter& decoded)
{
    if (!drivers::can::is_valid(frame)
        || frame.identifier < first_response_identifier
        || frame.identifier > last_response_identifier
        || frame.length < 4U
        || (frame.data[0U] & 0xF0U) != 0U) {
        return false;
    }

    const std::size_t payload_length = frame.data[0U];
    if (payload_length < 3U
        || payload_length + 1U > frame.length
        || frame.data[1U] != current_data_response_mode) {
        return false;
    }

    return decode_value(frame.data[2U], frame, decoded);
}

}  // пространство имён protocol::obd2
