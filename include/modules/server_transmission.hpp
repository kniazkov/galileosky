/**
 * @file server_transmission.hpp
 * @brief Объявляет модуль очереди сообщений для сервера.
 *
 * Модуль хранит ограниченный набор неотправленных сообщений и удаляет самое
 * старое при переполнении.
 */

#pragma once

#include "drivers/server_transport.hpp"

#include <cstdint>

namespace modules::server_transmission {

constexpr std::uint8_t queue_capacity = 4U;

/**
 * @brief Содержит наблюдаемое состояние очереди и счётчики передачи.
 */
struct Snapshot {
    std::uint8_t queued_messages{};
    std::uint32_t dropped_messages{};
    std::uint32_t sent_messages{};
    std::uint32_t revision{};
};

/**
 * @brief Сбрасывает очередь и счётчики модуля.
 */
void reset();

/**
 * @brief Добавляет сообщение, удаляя самое старое при заполненной очереди.
 * @return false, если сообщение имеет недопустимый размер.
 */
bool enqueue(const drivers::server_transport::Message& message);

/**
 * @brief Пытается передать одно сообщение из головы очереди.
 * @return true, если в очереди остаётся немедленная работа.
 */
bool tick();

/**
 * @brief Возвращает состояние очереди передачи.
 */
Snapshot snapshot();

}  // пространство имён modules::server_transmission
