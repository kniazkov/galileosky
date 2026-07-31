/**
 * @file gnss.cpp
 * @brief Реализует управляемый приёмник GPS/ГЛОНАСС desktop-стенда.
 */

#include "drivers/gnss.hpp"
#include "platform/desktop/gnss_adapter.hpp"

namespace {

drivers::gnss::Fix current_fix{};
bool initialized{};

}  // безымянное пространство имён

namespace drivers::gnss {

void reset()
{
    current_fix = {};
    initialized = false;
}

bool read(Fix& fix)
{
    if (!initialized) {
        return false;
    }
    fix = current_fix;
    return true;
}

}  // пространство имён drivers::gnss

namespace platform::desktop_gnss {

bool set_fix(const drivers::gnss::Fix& fix)
{
    if (!drivers::gnss::is_valid(fix)) {
        return false;
    }
    current_fix = fix;
    initialized = true;
    return true;
}

}  // пространство имён platform::desktop_gnss
