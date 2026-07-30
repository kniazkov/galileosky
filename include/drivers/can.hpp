/**
 * @file can.hpp
 * @brief Задаёт общий интерфейс обмена стандартными кадрами CAN.
 *
 * Драйвер скрывает платформенную реализацию и работает только с 11-битными
 * идентификаторами и полезной нагрузкой классического CAN до восьми байт.
 */

#pragma once

#include <array>
#include <cstdint>

namespace drivers::can {

constexpr std::uint16_t maximum_identifier = 0x07FFU;
constexpr std::size_t maximum_data_length = 8U;

/**
 * @brief Представляет один стандартный кадр CAN 2.0.
 */
struct Frame {
    std::uint16_t identifier{};
    std::uint8_t length{};
    std::array<std::uint8_t, maximum_data_length> data{};
};

/**
 * @brief Проверяет границы идентификатора и длины кадра.
 */
bool is_valid(const Frame& frame);

/**
 * @brief Сбрасывает платформенное состояние CAN-драйвера.
 */
void reset();

/**
 * @brief Извлекает следующий принятый кадр.
 * @return true, если кадр был получен.
 */
bool receive(Frame& frame);

/**
 * @brief Передаёт кадр через платформенную реализацию.
 * @return true, если драйвер принял кадр к отправке.
 */
bool send(const Frame& frame);

}  // пространство имён drivers::can
