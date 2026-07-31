/**
 * @file service_port.hpp
 * @brief Задаёт общий двунаправленный интерфейс сервисного порта.
 *
 * Драйвер передаёт короткие бинарные кадры между устройством и конфигурирующим
 * компьютером, не фиксируя UART или другой физический интерфейс.
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace drivers::service_port {

constexpr std::size_t maximum_frame_size = 32U;

/**
 * @brief Представляет один запрос или ответ сервисного протокола.
 */
struct Frame {
    std::uint8_t length{};
    std::array<std::uint8_t, maximum_frame_size> data{};
};

/**
 * @brief Проверяет размер сервисного кадра.
 */
bool is_valid(const Frame& frame);

/**
 * @brief Сбрасывает платформенное состояние сервисного порта.
 */
void reset();

/**
 * @brief Получает один запрос от конфигурирующего компьютера.
 * @return true, если запрос был получен.
 */
bool receive(Frame& frame);

/**
 * @brief Передаёт один ответ конфигурирующему компьютеру.
 * @return true, если ответ принят драйвером.
 */
bool send(const Frame& frame);

}  // пространство имён drivers::service_port
