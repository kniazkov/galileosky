/**
 * @file service_port_adapter.hpp
 * @brief Открывает сервисный порт desktop-стенду.
 */

#pragma once

#include "drivers/service_port.hpp"

namespace platform::desktop_service_port {

/**
 * @brief Добавляет запрос во входную очередь сервисного порта.
 */
bool inject_received(const drivers::service_port::Frame& frame);

/**
 * @brief Извлекает самый старый ответ устройства.
 */
bool pop_transmitted(drivers::service_port::Frame& frame);

}  // пространство имён platform::desktop_service_port
