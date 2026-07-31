/**
 * @file main.cpp
 * @brief Содержит общую точку входа приложения.
 *
 * Файл не зависит от операционной системы или микроконтроллера. После запуска
 * состояние модулей явно сбрасывается, затем управление передаётся платформе.
 */

#include "application/application.hpp"
#include "platform/runtime.hpp"

int main()
{
    application::reset();
    return platform::run_application();
}
