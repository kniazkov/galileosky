/**
 * @file gnss.cpp
 * @brief Реализует общую проверку навигационной фиксации.
 */

#include "drivers/gnss.hpp"

namespace drivers::gnss {

bool is_valid(const Fix& fix)
{
    if (!fix.valid) {
        return true;
    }

    return fix.latitude_e7 >= minimum_latitude_e7
        && fix.latitude_e7 <= maximum_latitude_e7
        && fix.longitude_e7 >= minimum_longitude_e7
        && fix.longitude_e7 <= maximum_longitude_e7;
}

}  // пространство имён drivers::gnss
