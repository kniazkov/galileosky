/**
 * @file sensors.hpp
 * @brief Задаёт общий интерфейс чтения ADC и дискретных входов.
 *
 * Драйвер возвращает сырые 12-битные значения ADC и логические уровни GPIO,
 * не добавляя аппаратно-зависимую калибровку в прикладной код.
 */

#pragma once

#include <cstdint>

namespace drivers::sensors {

constexpr std::uint16_t maximum_adc_value = 4'095U;

/**
 * @brief Перечисляет доступные аналоговые каналы.
 */
enum class AnalogChannel : std::uint8_t {
    supply_voltage,
    external_input
};

/**
 * @brief Перечисляет доступные дискретные входы.
 */
enum class DigitalInput : std::uint8_t {
    ignition,
    door_open,
    alarm
};

/**
 * @brief Сбрасывает платформенное состояние датчиков.
 */
void reset();

/**
 * @brief Читает сырое значение аналогового канала.
 * @return true, если значение доступно.
 */
bool read_analog(AnalogChannel channel, std::uint16_t& value);

/**
 * @brief Читает логическое состояние дискретного входа.
 * @return true, если состояние доступно.
 */
bool read_digital(DigitalInput input, bool& active);

}  // пространство имён drivers::sensors
