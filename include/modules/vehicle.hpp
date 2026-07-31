/**
 * @file vehicle.hpp
 * @brief Объявляет модуль текущего состояния автомобиля.
 *
 * Модуль опрашивает OBD-II через общий CAN-драйвер и хранит небольшой набор
 * последних известных параметров в статической памяти.
 */

#pragma once

#include <cstdint>

namespace modules::vehicle {

constexpr std::uint8_t engine_speed_valid = 1U << 0U;
constexpr std::uint8_t vehicle_speed_valid = 1U << 1U;
constexpr std::uint8_t coolant_temperature_valid = 1U << 2U;
constexpr std::uint8_t fuel_level_valid = 1U << 3U;
constexpr std::uint8_t check_engine_valid = 1U << 4U;

/**
 * @brief Содержит последние известные параметры и маску их достоверности.
 */
struct Snapshot {
    std::uint16_t engine_speed_rpm{};
    std::uint32_t engine_speed_timestamp_ms{};
    std::uint8_t vehicle_speed_kmh{};
    std::int16_t coolant_temperature_c{};
    std::uint8_t fuel_level_percent{};
    bool check_engine{};
    std::uint8_t valid_mask{};
    std::uint32_t revision{};
};

/**
 * @brief Сбрасывает параметры и расписание опроса.
 */
void reset();

/**
 * @brief Обрабатывает входящие кадры и при необходимости запускает опрос.
 */
void tick(std::uint32_t timestamp_ms);

/**
 * @brief Возвращает текущий снимок состояния автомобиля.
 */
Snapshot snapshot();

}  // пространство имён modules::vehicle
