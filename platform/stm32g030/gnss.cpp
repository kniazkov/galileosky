/**
 * @file gnss.cpp
 * @brief Реализует границу GPS/ГЛОНАСС для целевой сборки.
 *
 * Модель приёмника и его UART-протокол не заданы, поэтому заглушка не создаёт
 * фиктивную навигационную фиксацию.
 */

#include "drivers/gnss.hpp"

namespace drivers::gnss {

void reset()
{
}

bool read(Fix& fix)
{
    (void)fix;
    return false;
}

}  // пространство имён drivers::gnss
