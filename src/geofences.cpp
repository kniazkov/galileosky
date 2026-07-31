/**
 * @file geofences.cpp
 * @brief Реализует три прямоугольные геозоны и события их пересечения.
 */

#include "modules/geofences.hpp"

#include "modules/event_messages.hpp"

#include <array>
#include <cstddef>

namespace {

constexpr std::uint8_t geofence_enter_event = 0x10U;
constexpr std::uint8_t geofence_exit_event = 0x11U;
constexpr std::uint8_t event_payload_size = 16U;

/**
 * @brief Описывает прямоугольную геозону в координатах градусов на 10^7.
 */
struct Zone {
    std::uint8_t id;
    std::int32_t minimum_latitude_e7;
    std::int32_t maximum_latitude_e7;
    std::int32_t minimum_longitude_e7;
    std::int32_t maximum_longitude_e7;
};

constexpr std::array<Zone, modules::geofences::zone_count> zones{{
    {1U, 580'000'000, 580'099'999, 560'000'000, 560'099'999},
    {2U, 580'200'000, 580'299'999, 560'200'000, 560'299'999},
    {3U, 580'400'000, 580'499'999, 560'400'000, 560'499'999}
}};

/**
 * @brief Хранит последнюю обработанную фиксацию и исходную принадлежность.
 */
struct GeofenceState {
    modules::geofences::Snapshot snapshot{};
    std::uint32_t last_navigation_revision{};
    bool navigation_seen{};
    bool baseline_ready{};
    bool enabled_seen{};
    bool enabled{};
};

GeofenceState state{};

bool contains(
    const Zone& zone,
    const modules::navigation::Snapshot& navigation)
{
    return navigation.latitude_e7 >= zone.minimum_latitude_e7
        && navigation.latitude_e7 <= zone.maximum_latitude_e7
        && navigation.longitude_e7 >= zone.minimum_longitude_e7
        && navigation.longitude_e7 <= zone.maximum_longitude_e7;
}

std::uint8_t calculate_inside_mask(
    const modules::navigation::Snapshot& navigation)
{
    std::uint8_t mask = 0U;
    for (std::size_t index = 0U; index < zones.size(); ++index) {
        if (contains(zones[index], navigation)) {
            mask |= static_cast<std::uint8_t>(1U << index);
        }
    }
    return mask;
}

void write_u16(
    std::array<std::uint8_t, event_payload_size>& payload,
    const std::size_t offset,
    const std::uint16_t value)
{
    payload[offset] = static_cast<std::uint8_t>(value);
    payload[offset + 1U] = static_cast<std::uint8_t>(value >> 8U);
}

void write_u32(
    std::array<std::uint8_t, event_payload_size>& payload,
    const std::size_t offset,
    const std::uint32_t value)
{
    payload[offset] = static_cast<std::uint8_t>(value);
    payload[offset + 1U] = static_cast<std::uint8_t>(value >> 8U);
    payload[offset + 2U] = static_cast<std::uint8_t>(value >> 16U);
    payload[offset + 3U] = static_cast<std::uint8_t>(value >> 24U);
}

void send_event(
    const std::uint8_t event_code,
    const Zone& zone,
    const std::uint32_t timestamp_ms,
    const modules::navigation::Snapshot& navigation)
{
    std::array<std::uint8_t, event_payload_size> payload{};
    payload[0U] = event_code;
    payload[1U] = zone.id;
    write_u32(payload, 2U, timestamp_ms);
    write_u32(
        payload,
        6U,
        static_cast<std::uint32_t>(navigation.latitude_e7));
    write_u32(
        payload,
        10U,
        static_cast<std::uint32_t>(navigation.longitude_e7));
    write_u16(payload, 14U, navigation.ground_speed_centi_kph);
    modules::event_messages::enqueue(payload.data(), event_payload_size);
}

void process_transitions(
    const std::uint8_t previous_mask,
    const std::uint8_t current_mask,
    const std::uint32_t timestamp_ms,
    const modules::navigation::Snapshot& navigation)
{
    for (std::size_t index = 0U; index < zones.size(); ++index) {
        const std::uint8_t bit =
            static_cast<std::uint8_t>(1U << index);
        const bool was_inside = (previous_mask & bit) != 0U;
        const bool is_inside = (current_mask & bit) != 0U;
        if (was_inside == is_inside) {
            continue;
        }
        send_event(
            is_inside ? geofence_enter_event : geofence_exit_event,
            zones[index],
            timestamp_ms,
            navigation);
    }
}

}  // безымянное пространство имён

namespace modules::geofences {

void reset()
{
    state = {};
}

void tick(
    const std::uint32_t timestamp_ms,
    const modules::navigation::Snapshot& navigation,
    const bool enabled)
{
    const bool enabled_changed =
        !state.enabled_seen || enabled != state.enabled;
    state.enabled_seen = true;
    state.enabled = enabled;

    if (!enabled) {
        state.baseline_ready = false;
        if (state.snapshot.position_valid) {
            state.snapshot.position_valid = false;
            ++state.snapshot.revision;
        }
        return;
    }

    if (!enabled_changed
        && state.navigation_seen
        && navigation.revision == state.last_navigation_revision) {
        return;
    }
    state.navigation_seen = true;
    state.last_navigation_revision = navigation.revision;

    if (!navigation.fix_valid) {
        state.baseline_ready = false;
        if (state.snapshot.position_valid) {
            state.snapshot.position_valid = false;
            ++state.snapshot.revision;
        }
        return;
    }

    const std::uint8_t current_mask = calculate_inside_mask(navigation);
    const std::uint8_t previous_mask = state.snapshot.inside_mask;
    if (state.baseline_ready) {
        process_transitions(
            previous_mask,
            current_mask,
            timestamp_ms,
            navigation);
    }

    state.baseline_ready = true;
    const bool changed =
        !state.snapshot.position_valid || previous_mask != current_mask;
    state.snapshot.position_valid = true;
    state.snapshot.inside_mask = current_mask;
    if (changed) {
        ++state.snapshot.revision;
    }
}

Snapshot snapshot()
{
    return state.snapshot;
}

}  // пространство имён modules::geofences
