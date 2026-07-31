/**
 * @file server_transport.hpp
 * @brief Задаёт общий интерфейс передачи сообщений на сервер.
 *
 * Драйвер скрывает конкретный канал связи и принимает готовые сообщения
 * ограниченного размера без динамического выделения памяти.
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace drivers::server_transport {

constexpr std::size_t maximum_payload_size = 32U;

/**
 * @brief Представляет одно прикладное сообщение для сервера.
 */
struct Message {
    std::uint32_t message_id{};
    std::uint8_t length{};
    std::array<std::uint8_t, maximum_payload_size> payload{};
};

/**
 * @brief Проверяет допустимый размер сообщения.
 */
bool is_valid(const Message& message);

/**
 * @brief Сбрасывает платформенное состояние транспорта.
 */
void reset();

/**
 * @brief Сообщает, может ли транспорт принять сообщение сейчас.
 */
bool available();

/**
 * @brief Передаёт сообщение платформенной реализации.
 * @return true, если транспорт принял сообщение.
 */
bool send(const Message& message);

}  // пространство имён drivers::server_transport
