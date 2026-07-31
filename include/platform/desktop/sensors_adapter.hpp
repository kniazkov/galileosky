/**
 * @file sensors_adapter.hpp
 * @brief Открывает значения desktop-драйвера датчиков сценарию.
 *
 * Установленное значение становится доступным общему модулю при следующем
 * плановом опросе.
 */

#pragma once

#include "drivers/sensors.hpp"

#include <cstdint>

namespace platform::desktop_sensors {

/**
 * @brief Устанавливает сырое значение аналогового канала.
 * @return false, если значение выходит за диапазон 12-битного ADC.
 */
bool set_analog(
    drivers::sensors::AnalogChannel channel,
    std::uint16_t value);

/**
 * @brief Устанавливает состояние дискретного входа.
 */
void set_digital(
    drivers::sensors::DigitalInput input,
    bool active);

}  // пространство имён platform::desktop_sensors
