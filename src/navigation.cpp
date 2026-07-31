/**
 * @file navigation.cpp
 * @brief Реализует периодический опрос приёмника GPS/ГЛОНАСС.
 */

#include "modules/navigation.hpp"

#include "drivers/gnss.hpp"

namespace {

constexpr std::uint32_t polling_interval_ms = 100U;

/**
 * @brief Хранит навигационный снимок и расписание опроса.
 */
struct NavigationState {
    modules::navigation::Snapshot snapshot{};
    std::uint32_t last_poll_timestamp_ms{};
    bool polling_started{};
};

NavigationState state{};

void apply_fix(const drivers::gnss::Fix& fix)
{
    if (!drivers::gnss::is_valid(fix)) {
        return;
    }

    modules::navigation::Snapshot next{};
    next.fix_valid = fix.valid;
    if (fix.valid) {
        next.latitude_e7 = fix.latitude_e7;
        next.longitude_e7 = fix.longitude_e7;
        next.ground_speed_centi_kph = fix.ground_speed_centi_kph;
    }

    const bool changed =
        next.latitude_e7 != state.snapshot.latitude_e7
        || next.longitude_e7 != state.snapshot.longitude_e7
        || next.ground_speed_centi_kph
            != state.snapshot.ground_speed_centi_kph
        || next.fix_valid != state.snapshot.fix_valid;
    if (!changed) {
        return;
    }

    next.revision = state.snapshot.revision + 1U;
    state.snapshot = next;
}

}  // безымянное пространство имён

namespace modules::navigation {

void reset()
{
    state = {};
}

void tick(
    const std::uint32_t timestamp_ms,
    const bool receiver_enabled)
{
    if (!receiver_enabled) {
        state.polling_started = false;
        if (state.snapshot.fix_valid) {
            const std::uint32_t revision =
                state.snapshot.revision + 1U;
            state.snapshot = {};
            state.snapshot.revision = revision;
        }
        return;
    }

    if (state.polling_started
        && timestamp_ms - state.last_poll_timestamp_ms < polling_interval_ms) {
        return;
    }

    state.polling_started = true;
    state.last_poll_timestamp_ms = timestamp_ms;

    drivers::gnss::Fix fix{};
    if (drivers::gnss::read(fix)) {
        apply_fix(fix);
    }
}

Snapshot snapshot()
{
    return state.snapshot;
}

}  // пространство имён modules::navigation
