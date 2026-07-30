/**
 * @file application.cpp
 * @brief Реализует общий шаг приложения и его статическое состояние.
 *
 * Текущий каркас фиксирует последнее логическое время и номер ревизии. Здесь
 * последовательно подключаются прикладные модули без динамического выделения
 * памяти и без платформенных зависимостей.
 */

#include "application/application.hpp"

namespace {

struct ApplicationState {
    std::uint32_t timestamp_ms;
    std::uint32_t revision;
    bool initialized;
};

ApplicationState state{};

}  // безымянное пространство имён

namespace application {

void reset()
{
    state = {};
}

TickResult tick(const std::uint32_t timestamp_ms)
{
    if (!state.initialized || state.timestamp_ms != timestamp_ms) {
        state.timestamp_ms = timestamp_ms;
        ++state.revision;
        state.initialized = true;
    }

    return TickResult{false};
}

StateSnapshot snapshot()
{
    return StateSnapshot{state.timestamp_ms, state.revision};
}

}  // пространство имён application
