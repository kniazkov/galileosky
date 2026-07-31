/**
 * @file diagnostics.cpp
 * @brief Реализует счётчики, кольцевой журнал и контроль heartbeat.
 */

#include "modules/diagnostics.hpp"

#include <array>
#include <cstddef>

namespace {

/**
 * @brief Хранит диагностические данные и кольцевой журнал.
 */
struct DiagnosticState {
    modules::diagnostics::Snapshot snapshot{};
    std::array<
        modules::diagnostics::LogEntry,
        modules::diagnostics::log_capacity> log{};
    std::uint32_t last_heartbeat_timestamp_ms{};
    std::uint8_t log_head{};
    bool watchdog_started{};
};

DiagnosticState state{};

std::uint8_t next_index(const std::uint8_t index)
{
    return static_cast<std::uint8_t>(
        (index + 1U) % modules::diagnostics::log_capacity);
}

void append_log(
    const std::uint32_t timestamp_ms,
    const modules::diagnostics::LogCode code,
    const std::uint32_t argument)
{
    if (state.snapshot.log_entries == modules::diagnostics::log_capacity) {
        state.log_head = next_index(state.log_head);
        --state.snapshot.log_entries;
        ++state.snapshot.dropped_log_entries;
    }

    std::uint8_t tail = state.log_head;
    for (std::uint8_t offset = 0U;
         offset < state.snapshot.log_entries;
         ++offset) {
        tail = next_index(tail);
    }

    state.log[tail] =
        modules::diagnostics::LogEntry{timestamp_ms, code, argument};
    ++state.snapshot.log_entries;
    ++state.snapshot.revision;
}

}  // безымянное пространство имён

namespace modules::diagnostics {

void reset()
{
    state = {};
}

void tick(
    const std::uint32_t timestamp_ms,
    const std::uint32_t watchdog_timeout_ms)
{
    if (!state.watchdog_started) {
        state.watchdog_started = true;
        state.last_heartbeat_timestamp_ms = timestamp_ms;
        state.snapshot.heartbeat_count = 1U;
        state.snapshot.watchdog_healthy = true;
        ++state.snapshot.revision;
        return;
    }

    if (timestamp_ms == state.last_heartbeat_timestamp_ms) {
        return;
    }

    const std::uint32_t elapsed_ms =
        timestamp_ms - state.last_heartbeat_timestamp_ms;
    state.last_heartbeat_timestamp_ms = timestamp_ms;
    ++state.snapshot.heartbeat_count;

    state.snapshot.watchdog_healthy =
        elapsed_ms <= watchdog_timeout_ms;
    if (!state.snapshot.watchdog_healthy) {
        ++state.snapshot.watchdog_trips;
        append_log(
            timestamp_ms,
            LogCode::watchdog_timeout,
            elapsed_ms);
    } else {
        ++state.snapshot.revision;
    }
}

void record_configuration_update(
    const std::uint32_t timestamp_ms,
    const std::uint32_t configuration_revision)
{
    ++state.snapshot.configuration_updates;
    append_log(
        timestamp_ms,
        LogCode::configuration_changed,
        configuration_revision);
}

void record_configuration_rejection(
    const std::uint32_t timestamp_ms,
    const std::uint32_t reason)
{
    ++state.snapshot.configuration_rejections;
    append_log(
        timestamp_ms,
        LogCode::configuration_rejected,
        reason);
}

void record_protocol_error(
    const std::uint32_t timestamp_ms,
    const std::uint32_t command)
{
    ++state.snapshot.protocol_errors;
    append_log(
        timestamp_ms,
        LogCode::service_protocol_error,
        command);
}

Snapshot snapshot()
{
    return state.snapshot;
}

bool read_log(const std::uint8_t index, LogEntry& entry)
{
    if (index >= state.snapshot.log_entries) {
        return false;
    }

    std::uint8_t physical_index = state.log_head;
    for (std::uint8_t offset = 0U; offset < index; ++offset) {
        physical_index = next_index(physical_index);
    }
    entry = state.log[physical_index];
    return true;
}

}  // пространство имён modules::diagnostics
