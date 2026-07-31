/**
 * @file application.hpp
 * @brief Описывает общий детерминированный интерфейс прикладной логики.
 *
 * Функция tick вызывается одинаково в целевой и desktop-сборках. Всё состояние
 * приложения хранится статически внутри реализации, а наружу выдаётся только
 * компактный снимок, пригодный для контроля изменений.
 */

#pragma once

#include "modules/geofences.hpp"
#include "modules/navigation.hpp"
#include "modules/server_transmission.hpp"
#include "modules/sensors.hpp"
#include "modules/vehicle.hpp"

#include <cstdint>

namespace application {

/**
 * @brief Сообщает платформенному циклу, осталась ли немедленная работа.
 */
struct TickResult {
    bool work_pending;
};

/**
 * @brief Содержит наблюдаемую часть состояния приложения.
 */
struct StateSnapshot {
    std::uint32_t timestamp_ms;
    std::uint32_t revision;
    modules::vehicle::Snapshot vehicle;
    modules::sensors::Snapshot sensors;
    modules::navigation::Snapshot navigation;
    modules::geofences::Snapshot geofences;
    modules::server_transmission::Snapshot server_transmission;
};

/**
 * @brief Возвращает приложение в исходное состояние.
 */
void reset();

/**
 * @brief Выполняет один детерминированный шаг прикладной логики.
 * @param timestamp_ms Текущее монотонное время в миллисекундах.
 * @return Признак необходимости повторного шага без продвижения времени.
 */
TickResult tick(std::uint32_t timestamp_ms);

/**
 * @brief Возвращает состояние, доступное платформенному адаптеру.
 */
StateSnapshot snapshot();

}  // пространство имён application
