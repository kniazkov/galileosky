/**
 * @file gnss_adapter.hpp
 * @brief Открывает desktop-приёмник GPS/ГЛОНАСС сценарию.
 */

#pragma once

#include "drivers/gnss.hpp"

namespace platform::desktop_gnss {

/**
 * @brief Устанавливает состояние навигационной фиксации.
 * @return false, если достоверные координаты выходят за допустимые границы.
 */
bool set_fix(const drivers::gnss::Fix& fix);

}  // пространство имён platform::desktop_gnss
