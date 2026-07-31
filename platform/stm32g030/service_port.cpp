/**
 * @file service_port.cpp
 * @brief Реализует границу сервисного порта для целевой сборки.
 *
 * Выбранный UART и его параметры в задании отсутствуют, поэтому заглушка не
 * принимает и не подтверждает фиктивные сервисные кадры.
 */

#include "drivers/service_port.hpp"

namespace drivers::service_port {

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

}  // пространство имён drivers::service_port
