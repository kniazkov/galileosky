/**
 * @file can_adapter.hpp
 * @brief Открывает очереди desktop CAN-драйвера сценарному стенду.
 *
 * Адаптер позволяет подать входящий кадр извне и считать кадр, который
 * приложение попыталось отправить.
 */

#pragma once

#include "drivers/can.hpp"

namespace platform::desktop_can {

/**
 * @brief Помещает кадр во входную очередь desktop-драйвера.
 * @return false для некорректного кадра или заполненной очереди.
 */
bool inject_received(const drivers::can::Frame& frame);

/**
 * @brief Извлекает следующий отправленный приложением кадр.
 * @return true, если выходная очередь не пуста.
 */
bool pop_transmitted(drivers::can::Frame& frame);

}  // пространство имён platform::desktop_can
