/**
 * @file navigation.hpp
 * @brief Объявляет модуль периодического опроса GPS/ГЛОНАСС.
 *
 * Координаты хранятся целыми числами в градусах, умноженных на 10^7, а
 * путевая скорость — в сотых долях километра в час.
 */

#pragma once

#include <cstdint>

namespace modules::navigation {

/**
 * @brief Содержит последнее нормализованное состояние навигации.
 */
struct Snapshot {
    std::int32_t latitude_e7{};
    std::int32_t longitude_e7{};
    std::uint16_t ground_speed_centi_kph{};
    bool fix_valid{};
    std::uint32_t revision{};
};

/**
 * @brief Сбрасывает фиксацию и расписание опроса.
 */
void reset();

/**
 * @brief Опрашивает приёмник при наступлении 100-миллисекундного интервала.
 */
void tick(std::uint32_t timestamp_ms);

/**
 * @brief Возвращает текущее состояние навигации.
 */
Snapshot snapshot();

}  // пространство имён modules::navigation
