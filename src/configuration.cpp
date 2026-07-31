/**
 * @file configuration.cpp
 * @brief Реализует настройки и компактный бинарный сервисный протокол.
 */

#include "modules/configuration.hpp"

#include "drivers/service_port.hpp"
#include "modules/diagnostics.hpp"

#include <cstddef>

namespace {

constexpr std::uint8_t read_configuration_command = 0x01U;
constexpr std::uint8_t write_configuration_command = 0x02U;
constexpr std::uint8_t read_diagnostics_command = 0x03U;
constexpr std::uint8_t read_log_command = 0x04U;
constexpr std::uint8_t response_mask = 0x80U;
constexpr std::uint8_t error_response = 0xFFU;

constexpr std::uint8_t status_ok = 0U;
constexpr std::uint8_t status_invalid = 1U;
constexpr std::uint8_t status_unsupported = 2U;
constexpr std::uint8_t status_not_found = 3U;

modules::configuration::Snapshot active_configuration{};

std::uint32_t read_u32(
    const drivers::service_port::Frame& frame,
    const std::size_t offset)
{
    return static_cast<std::uint32_t>(frame.data[offset])
        | (static_cast<std::uint32_t>(frame.data[offset + 1U]) << 8U)
        | (static_cast<std::uint32_t>(frame.data[offset + 2U]) << 16U)
        | (static_cast<std::uint32_t>(frame.data[offset + 3U]) << 24U);
}

void write_u32(
    drivers::service_port::Frame& frame,
    const std::size_t offset,
    const std::uint32_t value)
{
    frame.data[offset] = static_cast<std::uint8_t>(value);
    frame.data[offset + 1U] = static_cast<std::uint8_t>(value >> 8U);
    frame.data[offset + 2U] = static_cast<std::uint8_t>(value >> 16U);
    frame.data[offset + 3U] = static_cast<std::uint8_t>(value >> 24U);
}

void send_status(
    const std::uint8_t response,
    const std::uint8_t status)
{
    drivers::service_port::Frame frame{};
    frame.length = 2U;
    frame.data[0U] = response;
    frame.data[1U] = status;
    drivers::service_port::send(frame);
}

void send_configuration()
{
    drivers::service_port::Frame frame{};
    frame.length = 7U;
    frame.data[0U] = read_configuration_command | response_mask;
    frame.data[1U] = status_ok;
    frame.data[2U] =
        active_configuration.geofences_enabled ? 1U : 0U;
    write_u32(
        frame,
        3U,
        active_configuration.watchdog_timeout_ms);
    drivers::service_port::send(frame);
}

void write_configuration(
    const std::uint32_t timestamp_ms,
    const drivers::service_port::Frame& request)
{
    if (request.length != 6U
        || request.data[1U] > 1U) {
        modules::diagnostics::record_configuration_rejection(
            timestamp_ms,
            status_invalid);
        send_status(
            write_configuration_command | response_mask,
            status_invalid);
        return;
    }

    const std::uint32_t timeout_ms = read_u32(request, 2U);
    if (timeout_ms < modules::configuration::minimum_watchdog_timeout_ms
        || timeout_ms
            > modules::configuration::maximum_watchdog_timeout_ms) {
        modules::diagnostics::record_configuration_rejection(
            timestamp_ms,
            status_invalid);
        send_status(
            write_configuration_command | response_mask,
            status_invalid);
        return;
    }

    const bool geofences_enabled = request.data[1U] != 0U;
    const bool changed =
        geofences_enabled != active_configuration.geofences_enabled
        || timeout_ms != active_configuration.watchdog_timeout_ms;
    if (changed) {
        active_configuration.geofences_enabled = geofences_enabled;
        active_configuration.watchdog_timeout_ms = timeout_ms;
        ++active_configuration.revision;
        modules::diagnostics::record_configuration_update(
            timestamp_ms,
            active_configuration.revision);
    }

    send_status(
        write_configuration_command | response_mask,
        status_ok);
}

void send_diagnostics()
{
    const modules::diagnostics::Snapshot diagnostics =
        modules::diagnostics::snapshot();
    drivers::service_port::Frame frame{};
    frame.length = 28U;
    frame.data[0U] = read_diagnostics_command | response_mask;
    frame.data[1U] = status_ok;
    write_u32(frame, 2U, diagnostics.heartbeat_count);
    write_u32(frame, 6U, diagnostics.watchdog_trips);
    write_u32(frame, 10U, diagnostics.configuration_updates);
    write_u32(frame, 14U, diagnostics.configuration_rejections);
    write_u32(frame, 18U, diagnostics.protocol_errors);
    write_u32(frame, 22U, diagnostics.dropped_log_entries);
    frame.data[26U] = diagnostics.log_entries;
    frame.data[27U] = diagnostics.watchdog_healthy ? 1U : 0U;
    drivers::service_port::send(frame);
}

void send_log_entry(const drivers::service_port::Frame& request)
{
    if (request.length != 2U) {
        send_status(read_log_command | response_mask, status_invalid);
        return;
    }

    modules::diagnostics::LogEntry entry{};
    if (!modules::diagnostics::read_log(request.data[1U], entry)) {
        drivers::service_port::Frame frame{};
        frame.length = 3U;
        frame.data[0U] = read_log_command | response_mask;
        frame.data[1U] = status_not_found;
        frame.data[2U] = request.data[1U];
        drivers::service_port::send(frame);
        return;
    }

    drivers::service_port::Frame frame{};
    frame.length = 12U;
    frame.data[0U] = read_log_command | response_mask;
    frame.data[1U] = status_ok;
    frame.data[2U] = request.data[1U];
    write_u32(frame, 3U, entry.timestamp_ms);
    frame.data[7U] = static_cast<std::uint8_t>(entry.code);
    write_u32(frame, 8U, entry.argument);
    drivers::service_port::send(frame);
}

void process_request(
    const std::uint32_t timestamp_ms,
    const drivers::service_port::Frame& request)
{
    if (request.length == 0U) {
        modules::diagnostics::record_protocol_error(timestamp_ms, 0U);
        send_status(error_response, status_invalid);
        return;
    }

    const std::uint8_t command = request.data[0U];
    switch (command) {
    case read_configuration_command:
        if (request.length == 1U) {
            send_configuration();
        } else {
            modules::diagnostics::record_protocol_error(
                timestamp_ms,
                command);
            send_status(command | response_mask, status_invalid);
        }
        return;
    case write_configuration_command:
        write_configuration(timestamp_ms, request);
        return;
    case read_diagnostics_command:
        if (request.length == 1U) {
            send_diagnostics();
        } else {
            modules::diagnostics::record_protocol_error(
                timestamp_ms,
                command);
            send_status(command | response_mask, status_invalid);
        }
        return;
    case read_log_command:
        send_log_entry(request);
        return;
    default:
        modules::diagnostics::record_protocol_error(
            timestamp_ms,
            command);
        send_status(error_response, status_unsupported);
        return;
    }
}

}  // безымянное пространство имён

namespace modules::configuration {

void reset()
{
    active_configuration = {};
}

bool tick(const std::uint32_t timestamp_ms)
{
    drivers::service_port::Frame request{};
    if (!drivers::service_port::receive(request)) {
        return false;
    }

    process_request(timestamp_ms, request);
    return true;
}

Snapshot snapshot()
{
    return active_configuration;
}

}  // пространство имён modules::configuration
