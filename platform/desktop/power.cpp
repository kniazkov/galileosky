/**
 * @file power.cpp
 * @brief Реализует управляемый домен питания для desktop-стенда.
 *
 * Эмулятор запоминает команду общего модуля без обращения к реальному GPIO.
 */

#include "drivers/power.hpp"

namespace {

bool gnss_enabled{};

}  // безымянное пространство имён

namespace drivers::power {

void reset()
{
    gnss_enabled = false;
}

void set_enabled(const Domain domain, const bool enabled)
{
    if (domain == Domain::gnss) {
        gnss_enabled = enabled;
    }
}

}  // пространство имён drivers::power
