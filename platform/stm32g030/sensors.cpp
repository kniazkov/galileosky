/**
 * @file sensors.cpp
 * @brief Реализует границу датчиков для целевой сборки.
 *
 * Назначение выводов и схема ADC в задании не определены, поэтому заглушка не
 * возвращает фиктивные измерения.
 */

#include "drivers/sensors.hpp"

namespace drivers::sensors {

void reset()
{
}

bool read_analog(const AnalogChannel channel, std::uint16_t& value)
{
    (void)channel;
    (void)value;
    return false;
}

bool read_digital(const DigitalInput input, bool& active)
{
    (void)input;
    (void)active;
    return false;
}

}  // пространство имён drivers::sensors
