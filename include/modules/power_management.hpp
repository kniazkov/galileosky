/**
 * @file power_management.hpp
 * @brief Объявляет экономичную политику питания внешних модулей.
 *
 * Пока двигатель работает, навигационный приёмник включён постоянно. При
 * остановленном двигателе он просыпается короткими периодическими окнами.
 */

#pragma once

#include "modules/vehicle.hpp"

#include <cstdint>

namespace modules::power_management {

constexpr std::uint32_t gnss_active_window_ms = 10'000U;
constexpr std::uint32_t gnss_sleep_interval_ms = 60'000U;
constexpr std::uint32_t engine_status_timeout_ms = 5'000U;

/**
 * @brief Описывает текущий режим питания навигационного приёмника.
 */
enum class Mode : std::uint8_t {
    engine_running,
    periodic_fix,
    sleeping
};

/**
 * @brief Содержит наблюдаемое состояние политики питания.
 */
struct Snapshot {
    bool gnss_enabled{};
    bool engine_running{};
    Mode mode{Mode::sleeping};
    std::uint32_t transitions{};
    std::uint32_t revision{};
};

/**
 * @brief Сбрасывает расписание и наблюдаемое состояние модуля.
 */
void reset();

/**
 * @brief Обновляет питание с учётом времени и оборотов двигателя.
 */
void tick(
    std::uint32_t timestamp_ms,
    const modules::vehicle::Snapshot& vehicle);

/**
 * @brief Возвращает текущее состояние политики питания.
 */
Snapshot snapshot();

}  // пространство имён modules::power_management
