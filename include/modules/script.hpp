/**
 * @file script.hpp
 * @brief Объявляет ограниченный построчный интерпретатор BASIC.
 *
 * Программа и переменные находятся в статической памяти. Язык не содержит
 * циклов и пользовательских функций, поэтому один запуск всегда ограничен
 * размером исходного текста.
 */

#pragma once

#include <cstddef>
#include <cstdint>

namespace modules::script {

constexpr std::size_t maximum_program_size = 1'024U;
constexpr std::size_t maximum_line_length = 96U;
constexpr std::uint8_t maximum_variables = 8U;
constexpr std::uint8_t maximum_name_length = 8U;
constexpr std::uint8_t maximum_if_depth = 4U;
constexpr std::uint8_t maximum_expression_depth = 2U;
constexpr std::uint32_t execution_interval_ms = 100U;

/**
 * @brief Перечисляет проверяемые ошибки программы и выполнения.
 */
enum class Error : std::uint8_t {
    none = 0U,
    program_too_large = 1U,
    line_too_long = 2U,
    syntax = 3U,
    name_too_long = 4U,
    variable_limit = 5U,
    access_denied = 6U,
    division_by_zero = 7U,
    numeric_range = 8U,
    nesting_limit = 9U,
    block_mismatch = 10U,
    send_failed = 11U
};

/**
 * @brief Содержит состояние загруженной программы и её счётчики.
 */
struct Snapshot {
    bool loaded{};
    bool faulted{};
    Error error{};
    std::uint16_t error_line{};
    std::uint16_t program_size{};
    std::uint8_t variables{};
    std::uint32_t executions{};
    std::uint32_t sent_messages{};
    std::uint32_t revision{};
};

/**
 * @brief Загружает демонстрационную программу контроля скорости.
 */
void reset();

/**
 * @brief Копирует и проверяет новую программу, сбрасывая её переменные.
 * @return true, если программа прошла синтаксическую проверку.
 */
bool load(const char* program, std::size_t length);

/**
 * @brief Выполняет программу при изменении времени или входных модулей.
 */
void tick(std::uint32_t timestamp_ms);

/**
 * @brief Возвращает состояние интерпретатора.
 */
Snapshot snapshot();

}  // пространство имён modules::script
