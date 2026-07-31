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
#include "drivers/server_transport.hpp"
#include "drivers/sensors.hpp"
#include "modules/sensor_events.hpp"
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
    drivers::server_transport::reset();
    drivers::sensors::reset();
    modules::sensor_events::reset();
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

    modules::vehicle::tick(timestamp_ms);
    modules::sensors::tick(timestamp_ms);
    modules::sensor_events::tick(timestamp_ms, modules::sensors::snapshot());
    return TickResult{modules::server_transmission::tick()};
}

StateSnapshot snapshot()
{
    return StateSnapshot{
        state.timestamp_ms,
        state.revision,
        modules::vehicle::snapshot(),
        modules::sensors::snapshot(),
        modules::server_transmission::snapshot()};
}

}  // пространство имён application
