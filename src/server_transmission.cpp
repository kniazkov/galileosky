/**
 * @file server_transmission.cpp
 * @brief Реализует ограниченную очередь сообщений для сервера.
 *
 * Все сообщения и счётчики находятся в статической памяти. Успешная передача
 * удаляет голову очереди, а переполнение сначала удаляет самую старую запись.
 */

#include "modules/server_transmission.hpp"

#include <array>
#include <cstddef>

namespace {

/**
 * @brief Хранит кольцевую очередь и наблюдаемые счётчики модуля.
 */
struct TransmissionState {
    std::array<
        drivers::server_transport::Message,
        modules::server_transmission::queue_capacity> messages{};
    modules::server_transmission::Snapshot snapshot{};
    std::uint8_t head{};
};

TransmissionState state{};

std::uint8_t next_index(const std::uint8_t index)
{
    return static_cast<std::uint8_t>(
        (index + 1U) % modules::server_transmission::queue_capacity);
}

}  // безымянное пространство имён

namespace modules::server_transmission {

void reset()
{
    state = {};
}

bool enqueue(const drivers::server_transport::Message& message)
{
    if (!drivers::server_transport::is_valid(message)) {
        return false;
    }

    if (state.snapshot.queued_messages == queue_capacity) {
        state.head = next_index(state.head);
        --state.snapshot.queued_messages;
        ++state.snapshot.dropped_messages;
    }

    std::uint8_t tail = state.head;
    for (std::uint8_t offset = 0U;
         offset < state.snapshot.queued_messages;
         ++offset) {
        tail = next_index(tail);
    }

    state.messages[tail] = message;
    ++state.snapshot.queued_messages;
    ++state.snapshot.revision;
    return true;
}

bool tick()
{
    if (state.snapshot.queued_messages == 0U
        || !drivers::server_transport::available()) {
        return false;
    }

    if (!drivers::server_transport::send(state.messages[state.head])) {
        return false;
    }

    state.head = next_index(state.head);
    --state.snapshot.queued_messages;
    ++state.snapshot.sent_messages;
    ++state.snapshot.revision;
    return state.snapshot.queued_messages != 0U;
}

Snapshot snapshot()
{
    return state.snapshot;
}

}  // пространство имён modules::server_transmission
