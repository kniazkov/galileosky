/**
 * @file gnss.hpp
 * @brief Задаёт общий интерфейс приёмника GPS/ГЛОНАСС.
 *
 * Драйвер возвращает уже нормализованную навигационную фиксацию. Конкретный
 * UART-протокол, NMEA или команды выбранного приёмника остаются платформе.
 */

#pragma once

#include <cstdint>

namespace drivers::gnss {

constexpr std::int32_t minimum_latitude_e7 = -900'000'000;
constexpr std::int32_t maximum_latitude_e7 = 900'000'000;
constexpr std::int32_t minimum_longitude_e7 = -1'800'000'000;
constexpr std::int32_t maximum_longitude_e7 = 1'800'000'000;

/**
 * @brief Содержит координаты и путевую скорость одной фиксации.
 */
struct Fix {
    std::int32_t latitude_e7{};
    std::int32_t longitude_e7{};
    std::uint16_t ground_speed_centi_kph{};
    bool valid{};
};

/**
 * @brief Проверяет диапазоны координат достоверной фиксации.
 */
bool is_valid(const Fix& fix);

/**
 * @brief Сбрасывает платформенное состояние приёмника.
 */
void reset();

/**
 * @brief Читает последнее состояние навигационной фиксации.
 * @return true, если приёмник уже сообщил состояние фиксации.
 */
bool read(Fix& fix);

}  // пространство имён drivers::gnss
