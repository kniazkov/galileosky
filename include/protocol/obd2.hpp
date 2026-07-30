/**
 * @file obd2.hpp
 * @brief Описывает общий декодер автомобильных параметров OBD-II поверх CAN.
 *
 * Код формирует диагностические запросы режима 01 и разбирает однофреймовые
 * ответы электронных блоков управления.
 */

#pragma once

#include "drivers/can.hpp"

#include <cstdint>

namespace protocol::obd2 {

/**
 * @brief Перечисляет параметры автомобиля, используемые приложением.
 */
enum class Parameter : std::uint8_t {
    engine_speed,
    vehicle_speed,
    coolant_temperature,
    fuel_level,
    check_engine
};

/**
 * @brief Содержит распознанный параметр и его целочисленное значение.
 */
struct DecodedParameter {
    Parameter parameter;
    std::int32_t value;
};

/**
 * @brief Формирует широковещательный запрос текущего значения параметра.
 */
drivers::can::Frame make_request(Parameter parameter);

/**
 * @brief Разбирает однофреймовый ответ OBD-II режима 01.
 * @return true, если кадр содержит один из поддерживаемых параметров.
 */
bool decode(
    const drivers::can::Frame& frame,
    DecodedParameter& decoded);

}  // пространство имён protocol::obd2
