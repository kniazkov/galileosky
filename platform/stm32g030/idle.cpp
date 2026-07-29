/**
 * @file idle.cpp
 * @brief Реализует пустой бесконечный цикл для STM32G030K6.
 *
 * Функция не обращается к периферии и непрерывно выполняет инструкцию nop.
 * Она используется как минимальная рабочая нагрузка bare-metal прошивки.
 */

#include "platform/idle.hpp"

namespace platform {

[[noreturn]] void idle_forever()
{
    for (;;) {
        __asm volatile ("nop");
    }
}

}  // namespace platform
