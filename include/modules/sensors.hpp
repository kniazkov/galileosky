/**
 * @file sensors.hpp
 * @brief Объявляет модуль периодического опроса датчиков.
 *
 * Модуль раз в 100 миллисекунд считывает два ADC-канала и три дискретных
 * входа, сохраняя последние значения и маску их достоверности.
 */

#pragma once

#include <cstdint>

namespace modules::sensors {

constexpr std::uint8_t supply_voltage_valid = 1U << 0U;
constexpr std::uint8_t fuel_level_valid = 1U << 1U;
constexpr std::uint8_t ignition_valid = 1U << 2U;
constexpr std::uint8_t door_open_valid = 1U << 3U;
constexpr std::uint8_t alarm_valid = 1U << 4U;

/**
 * @brief Содержит последние значения и признаки их достоверности.
 */
struct Snapshot {
    std::uint16_t supply_voltage_adc{};
    std::uint16_t fuel_level_adc{};
    bool ignition{};
    bool door_open{};
    bool alarm{};
    std::uint8_t valid_mask{};
    std::uint32_t revision{};
};

/**
 * @brief Сбрасывает значения и расписание опроса.
 */
void reset();

/**
 * @brief Выполняет опрос, если наступил очередной 100-миллисекундный интервал.
 */
void tick(std::uint32_t timestamp_ms);

/**
 * @brief Возвращает текущий снимок датчиков.
 */
Snapshot snapshot();

}  // пространство имён modules::sensors
