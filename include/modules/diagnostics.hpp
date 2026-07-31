/**
 * @file diagnostics.hpp
 * @brief Объявляет диагностику, короткий журнал и программный watchdog.
 *
 * Модуль хранит только счётчики и четыре последние записи, не выделяя память
 * динамически.
 */

#pragma once

#include <cstdint>

namespace modules::diagnostics {

constexpr std::uint8_t log_capacity = 4U;

/**
 * @brief Перечисляет события компактного диагностического журнала.
 */
enum class LogCode : std::uint8_t {
    configuration_changed = 1U,
    configuration_rejected = 2U,
    service_protocol_error = 3U,
    watchdog_timeout = 4U
};

/**
 * @brief Содержит одну запись диагностического журнала.
 */
struct LogEntry {
    std::uint32_t timestamp_ms{};
    LogCode code{};
    std::uint32_t argument{};
};

/**
 * @brief Содержит диагностические счётчики и состояние watchdog.
 */
struct Snapshot {
    std::uint32_t heartbeat_count{};
    std::uint32_t watchdog_trips{};
    std::uint32_t configuration_updates{};
    std::uint32_t configuration_rejections{};
    std::uint32_t protocol_errors{};
    std::uint32_t dropped_log_entries{};
    std::uint8_t log_entries{};
    bool watchdog_healthy{};
    std::uint32_t revision{};
};

/**
 * @brief Сбрасывает счётчики, журнал и состояние watchdog.
 */
void reset();

/**
 * @brief Фиксирует очередной вызов приложения и проверяет задержку heartbeat.
 */
void tick(std::uint32_t timestamp_ms, std::uint32_t watchdog_timeout_ms);

/**
 * @brief Записывает успешное изменение конфигурации.
 */
void record_configuration_update(
    std::uint32_t timestamp_ms,
    std::uint32_t configuration_revision);

/**
 * @brief Записывает отклонённую конфигурацию.
 */
void record_configuration_rejection(
    std::uint32_t timestamp_ms,
    std::uint32_t reason);

/**
 * @brief Записывает ошибку сервисного протокола.
 */
void record_protocol_error(
    std::uint32_t timestamp_ms,
    std::uint32_t command);

/**
 * @brief Возвращает текущие диагностические счётчики.
 */
Snapshot snapshot();

/**
 * @brief Возвращает запись журнала по индексу от самой старой.
 */
bool read_log(std::uint8_t index, LogEntry& entry);

}  // пространство имён modules::diagnostics
