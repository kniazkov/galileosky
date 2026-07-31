/**
 * @file power.cpp
 * @brief Оставляет аппаратную границу управления питанием STM32G030.
 *
 * Схема внешнего ключа, управляющий вывод и полярность не заданы. Реализация
 * принимает команду политики, но не обращается к вымышленным регистрам платы.
 */

#include "drivers/power.hpp"

namespace drivers::power {

void reset()
{
}

void set_enabled(const Domain domain, const bool enabled)
{
    (void)domain;
    (void)enabled;
}

}  // пространство имён drivers::power
