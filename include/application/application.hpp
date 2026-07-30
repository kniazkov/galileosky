/**
 * @file application.hpp
 * @brief Описывает общий детерминированный интерфейс прикладной логики.
 *
 * Функция tick вызывается одинаково в целевой и desktop-сборках. Всё состояние
 * приложения хранится статически внутри реализации, а наружу выдаётся только
 * компактный снимок, пригодный для контроля изменений.
 */

#pragma once

#include <cstdint>

namespace application {

struct TickResult {
    bool work_pending;
};

struct StateSnapshot {
    std::uint32_t timestamp_ms;
    std::uint32_t revision;
};

void reset();
TickResult tick(std::uint32_t timestamp_ms);
StateSnapshot snapshot();

}  // пространство имён application
