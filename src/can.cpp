/**
 * @file can.cpp
 * @brief Реализует общую проверку стандартных кадров CAN.
 */

#include "drivers/can.hpp"

namespace drivers::can {

bool is_valid(const Frame& frame)
{
    return frame.identifier <= maximum_identifier
        && frame.length <= maximum_data_length;
}

}  // пространство имён drivers::can
