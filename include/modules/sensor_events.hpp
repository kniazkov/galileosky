/**
 * @file sensor_events.hpp
 * @brief Объявляет обработчик событий дискретных датчиков.
 *
 * Обработчик обнаруживает переходы из неактивного состояния в активное и
 * формирует сообщения для серверной очереди.
 */

#pragma once

#include "modules/sensors.hpp"

#include <cstdint>

namespace modules::sensor_events {

/**
 * @brief Сбрасывает запомненные состояния и нумерацию сообщений.
 */
void reset();

/**
 * @brief Обрабатывает фронты достоверных дискретных входов.
 */
void tick(
    std::uint32_t timestamp_ms,
    const modules::sensors::Snapshot& sensors);

}  // пространство имён modules::sensor_events
