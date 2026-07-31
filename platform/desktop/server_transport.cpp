/**
 * @file server_transport.cpp
 * @brief Реализует наблюдаемый серверный транспорт desktop-стенда.
 *
 * Доступность задаётся сценарием, а принятые сообщения сохраняются в
 * статической FIFO-очереди до выдачи в stdout.
 */

#include "drivers/server_transport.hpp"
#include "platform/desktop/server_transport_adapter.hpp"

#include <array>
#include <cstddef>

namespace {

constexpr std::size_t transmitted_queue_capacity = 16U;

/**
 * @brief Хранит отправленные сообщения без динамической памяти.
 */
class MessageQueue {
public:
    /**
     * @brief Добавляет сообщение в конец очереди.
     * @return false, если очередь заполнена.
     */
    bool push(const drivers::server_transport::Message& message)
    {
        if (size_ == messages_.size()) {
            return false;
        }

        messages_[tail_] = message;
        tail_ = (tail_ + 1U) % messages_.size();
        ++size_;
        return true;
    }

    /**
     * @brief Извлекает самое старое сообщение.
     * @return false, если очередь пуста.
     */
    bool pop(drivers::server_transport::Message& message)
    {
        if (size_ == 0U) {
            return false;
        }

        message = messages_[head_];
        head_ = (head_ + 1U) % messages_.size();
        --size_;
        return true;
    }

    /**
     * @brief Удаляет все накопленные сообщения.
     */
    void clear()
    {
        head_ = 0U;
        tail_ = 0U;
        size_ = 0U;
    }

private:
    std::array<
        drivers::server_transport::Message,
        transmitted_queue_capacity> messages_{};
    std::size_t head_{};
    std::size_t tail_{};
    std::size_t size_{};
};

MessageQueue transmitted_messages;
bool transport_available{};

}  // безымянное пространство имён

namespace drivers::server_transport {

void reset()
{
    transmitted_messages.clear();
    transport_available = false;
}

bool available()
{
    return transport_available;
}

bool send(const Message& message)
{
    return transport_available
        && is_valid(message)
        && transmitted_messages.push(message);
}

}  // пространство имён drivers::server_transport

namespace platform::desktop_server_transport {

void set_available(const bool available)
{
    transport_available = available;
}

bool pop_transmitted(drivers::server_transport::Message& message)
{
    return transmitted_messages.pop(message);
}

}  // пространство имён platform::desktop_server_transport
