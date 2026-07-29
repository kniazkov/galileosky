/**
 * @file idle.cpp
 * @brief Реализует цикл ожидания для desktop-сборок Windows и Linux.
 *
 * При запуске выводится сообщение, после чего процесс остаётся активным и
 * периодически засыпает, не занимая процессор непрерывным пустым циклом.
 */

#include "platform/idle.hpp"

#include <chrono>
#include <iostream>
#include <thread>

namespace platform {

[[noreturn]] void idle_forever()
{
    std::cout
        << "The Galileosky test project has been started. To stop it, press Ctrl+C."
        << std::endl;

    for (;;) {
        std::this_thread::sleep_for(std::chrono::seconds{1});
    }
}

}  // namespace platform
