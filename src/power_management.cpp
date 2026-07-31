/**
 * @file power_management.cpp
 * @brief Реализует расписание питания навигационного приёмника.
 *
 * Состояние и таймеры имеют фиксированный размер. Интервалы вычисляются
 * беззнаковой разностью, поэтому штатно переживают переполнение таймера.
 */

#include "modules/power_management.hpp"

#include "drivers/power.hpp"

namespace {

/**
 * @brief Хранит снимок и начало текущего временного интервала.
 */
struct PowerManagementState {
    modules::power_management::Snapshot snapshot{};
    std::uint32_t phase_started_ms{};
    bool initialized{};
};

PowerManagementState state{};

bool engine_is_running(
    const std::uint32_t timestamp_ms,
    const modules::vehicle::Snapshot& vehicle)
{
    return (vehicle.valid_mask & modules::vehicle::engine_speed_valid) != 0U
        && vehicle.engine_speed_rpm != 0U
        && timestamp_ms - vehicle.engine_speed_timestamp_ms
            <= modules::power_management::engine_status_timeout_ms;
}

/**
 * @brief Применяет режим и при необходимости переключает домен питания.
 */
void apply(
    const bool engine_running,
    const modules::power_management::Mode mode,
    const bool gnss_enabled)
{
    modules::power_management::Snapshot next = state.snapshot;
    next.engine_running = engine_running;
    next.mode = mode;

    if (next.gnss_enabled != gnss_enabled) {
        drivers::power::set_enabled(
            drivers::power::Domain::gnss,
            gnss_enabled);
        next.gnss_enabled = gnss_enabled;
        ++next.transitions;
    }

    const bool changed =
        next.gnss_enabled != state.snapshot.gnss_enabled
        || next.engine_running != state.snapshot.engine_running
        || next.mode != state.snapshot.mode
        || next.transitions != state.snapshot.transitions;
    if (changed) {
        next.revision = state.snapshot.revision + 1U;
        state.snapshot = next;
    }
}

}  // безымянное пространство имён

namespace modules::power_management {

void reset()
{
    state = {};
}

void tick(
    const std::uint32_t timestamp_ms,
    const modules::vehicle::Snapshot& vehicle)
{
    const bool engine_running =
        engine_is_running(timestamp_ms, vehicle);
    if (!state.initialized) {
        state.initialized = true;
        state.phase_started_ms = timestamp_ms;
        apply(
            engine_running,
            engine_running ? Mode::engine_running : Mode::periodic_fix,
            true);
        return;
    }

    if (engine_running) {
        state.phase_started_ms = timestamp_ms;
        apply(true, Mode::engine_running, true);
        return;
    }

    if (state.snapshot.mode == Mode::engine_running) {
        state.phase_started_ms = timestamp_ms;
        apply(false, Mode::periodic_fix, true);
        return;
    }

    const std::uint32_t elapsed_ms =
        timestamp_ms - state.phase_started_ms;
    if (state.snapshot.mode == Mode::periodic_fix
        && elapsed_ms >= gnss_active_window_ms) {
        state.phase_started_ms = timestamp_ms;
        apply(false, Mode::sleeping, false);
        return;
    }

    if (state.snapshot.mode == Mode::sleeping
        && elapsed_ms >= gnss_sleep_interval_ms) {
        state.phase_started_ms = timestamp_ms;
        apply(false, Mode::periodic_fix, true);
    }
}

Snapshot snapshot()
{
    return state.snapshot;
}

}  // пространство имён modules::power_management
