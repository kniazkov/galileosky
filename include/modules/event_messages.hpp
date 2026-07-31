/**
 * @file event_messages.hpp
 * @brief Объявляет единый источник номеров прикладных событий.
 *
 * Модуль упаковывает полезную нагрузку в серверное сообщение, чтобы события
 * разных подсистем не создавали одинаковые идентификаторы.
 */

#pragma once

#include <cstdint>

namespace modules::event_messages {

/**
 * @brief Сбрасывает нумерацию событий.
 */
void reset();

/**
 * @brief Добавляет событие в очередь передачи на сервер.
 * @return false, если размер или указатель полезной нагрузки недопустимы.
 */
bool enqueue(const std::uint8_t* payload, std::uint8_t length);

}  // пространство имён modules::event_messages
