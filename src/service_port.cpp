/**
 * @file service_port.cpp
 * @brief Реализует общую проверку сервисных кадров.
 */

#include "drivers/service_port.hpp"

namespace drivers::service_port {

bool is_valid(const Frame& frame)
{
    return frame.length <= maximum_frame_size;
}

}  // пространство имён drivers::service_port
