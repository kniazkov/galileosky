/**
 * @file configuration.hpp
 * @brief Объявляет конфигурацию и обработчик сервисного протокола.
 *
 * Модуль атомарно проверяет и применяет настройки, а также обслуживает чтение
 * диагностики и журнала через общий сервисный драйвер.
 */

#pragma once

#include <cstdint>

namespace modules::configuration {

constexpr std::uint32_t default_watchdog_timeout_ms = 1'000U;
constexpr std::uint32_t minimum_watchdog_timeout_ms = 100U;
constexpr std::uint32_t maximum_watchdog_timeout_ms = 10'000U;

/**
 * @brief Содержит активную конфигурацию устройства.
 */
struct Snapshot {
    bool geofences_enabled{true};
    std::uint32_t watchdog_timeout_ms{default_watchdog_timeout_ms};
    std::uint32_t revision{};
};

/**
 * @brief Восстанавливает конфигурацию по умолчанию.
 */
void reset();

/**
 * @brief Изменяет состояние геозон из внутреннего прикладного модуля.
 * @return true, если активное значение изменилось.
 */
bool set_geofences_enabled(
    std::uint32_t timestamp_ms,
    bool enabled);

/**
 * @brief Обрабатывает один сервисный запрос.
 * @return true, если запрос был обработан и возможна следующая работа.
 */
bool tick(std::uint32_t timestamp_ms);

/**
 * @brief Возвращает активную конфигурацию.
 */
Snapshot snapshot();

}  // пространство имён modules::configuration
