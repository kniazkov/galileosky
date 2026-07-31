/**
 * @file geofences.hpp
 * @brief Объявляет движок трёх статических геозон.
 *
 * Движок отслеживает принадлежность достоверной координаты прямоугольным
 * областям и создаёт события входа и выхода.
 */

#pragma once

#include "modules/navigation.hpp"

#include <cstdint>

namespace modules::geofences {

constexpr std::uint8_t zone_count = 3U;

/**
 * @brief Содержит наблюдаемое состояние движка геозон.
 */
struct Snapshot {
    std::uint8_t inside_mask{};
    bool position_valid{};
    std::uint32_t revision{};
};

/**
 * @brief Сбрасывает принадлежность зонам и исходную точку наблюдения.
 */
void reset();

/**
 * @brief Обрабатывает новую навигационную фиксацию и формирует события.
 */
void tick(
    std::uint32_t timestamp_ms,
    const modules::navigation::Snapshot& navigation);

/**
 * @brief Возвращает текущую маску геозон.
 */
Snapshot snapshot();

}  // пространство имён modules::geofences
