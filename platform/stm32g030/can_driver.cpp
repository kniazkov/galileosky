/**
 * @file can_driver.cpp
 * @brief Реализует границу CAN-драйвера для целевой сборки.
 *
 * Конкретный внешний CAN-контроллер в задании не определён, поэтому реализация
 * не подтверждает отправку и не создаёт фиктивные входящие кадры.
 */

#include "drivers/can.hpp"

namespace drivers::can {

void reset()
{
}

bool receive(Frame& frame)
{
    (void)frame;
    return false;
}

bool send(const Frame& frame)
{
    (void)frame;
    return false;
}

}  // пространство имён drivers::can
