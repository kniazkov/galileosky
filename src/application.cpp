/**
 * @file application.cpp
 * @brief Реализует общий шаг приложения и его статическое состояние.
 *
 * Текущий каркас фиксирует последнее логическое время и номер ревизии. Здесь
 * последовательно подключаются прикладные модули без динамического выделения
 * памяти и без платформенных зависимостей.
 */

#include "application/application.hpp"

#include "drivers/can.hpp"
#include "drivers/gnss.hpp"
#include "drivers/power.hpp"
#include "drivers/service_port.hpp"
#include "drivers/server_transport.hpp"
#include "drivers/sensors.hpp"
#include "modules/configuration.hpp"
#include "modules/diagnostics.hpp"
#include "modules/event_messages.hpp"
#include "modules/geofences.hpp"
#include "modules/navigation.hpp"
#include "modules/power_management.hpp"
#include "modules/sensor_events.hpp"
#include "modules/script.hpp"
#include "modules/server_transmission.hpp"
#include "modules/sensors.hpp"
#include "modules/vehicle.hpp"

namespace {

/**
 * @brief Хранит всё текущее состояние минимального прикладного ядра.
 */
struct ApplicationState {
    std::uint32_t timestamp_ms;
    std::uint32_t revision;
    bool initialized;
};

ApplicationState state{};

}  // безымянное пространство имён

namespace application {

void reset()
{
    state = {};
    drivers::can::reset();
    drivers::gnss::reset();
    drivers::power::reset();
    drivers::service_port::reset();
    drivers::server_transport::reset();
    drivers::sensors::reset();
    modules::configuration::reset();
    modules::diagnostics::reset();
    modules::event_messages::reset();
    modules::geofences::reset();
    modules::navigation::reset();
    modules::power_management::reset();
    modules::sensor_events::reset();
    modules::script::reset();
    modules::server_transmission::reset();
    modules::sensors::reset();
    modules::vehicle::reset();
}

TickResult tick(const std::uint32_t timestamp_ms)
{
    if (!state.initialized || state.timestamp_ms != timestamp_ms) {
        state.timestamp_ms = timestamp_ms;
        ++state.revision;
        state.initialized = true;
    }

    const modules::configuration::Snapshot configuration =
        modules::configuration::snapshot();
    modules::diagnostics::tick(
        timestamp_ms,
        configuration.watchdog_timeout_ms);
    modules::vehicle::tick(timestamp_ms);
    modules::power_management::tick(
        timestamp_ms,
        modules::vehicle::snapshot());
    const modules::power_management::Snapshot power =
        modules::power_management::snapshot();
    modules::navigation::tick(timestamp_ms, power.gnss_enabled);
    modules::sensors::tick(timestamp_ms);
    modules::script::tick(timestamp_ms);
    modules::geofences::tick(
        timestamp_ms,
        modules::navigation::snapshot(),
        configuration.geofences_enabled);
    modules::sensor_events::tick(timestamp_ms, modules::sensors::snapshot());
    const bool service_pending =
        modules::configuration::tick(timestamp_ms);
    const bool transmission_pending =
        modules::server_transmission::tick();
    return TickResult{service_pending || transmission_pending};
}

StateSnapshot snapshot()
{
    return StateSnapshot{
        state.timestamp_ms,
        state.revision,
        modules::vehicle::snapshot(),
        modules::sensors::snapshot(),
        modules::navigation::snapshot(),
        modules::geofences::snapshot(),
        modules::server_transmission::snapshot()};
}

}  // пространство имён application
