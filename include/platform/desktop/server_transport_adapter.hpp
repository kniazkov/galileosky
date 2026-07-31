/**
 * @file server_transport_adapter.hpp
 * @brief Открывает desktop-транспорт сценарному стенду.
 *
 * Адаптер управляет доступностью условной сети и позволяет считать сообщения,
 * принятые транспортом от приложения.
 */

#pragma once

#include "drivers/server_transport.hpp"

namespace platform::desktop_server_transport {

/**
 * @brief Устанавливает доступность транспорта для сценария.
 */
void set_available(bool available);

/**
 * @brief Извлекает следующее отправленное приложением сообщение.
 * @return true, если выходная очередь не пуста.
 */
bool pop_transmitted(drivers::server_transport::Message& message);

}  // пространство имён platform::desktop_server_transport
