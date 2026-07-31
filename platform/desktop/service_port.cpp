/**
 * @file service_port.cpp
 * @brief Реализует сервисный порт desktop-стенда двумя статическими очередями.
 */

#include "drivers/service_port.hpp"
#include "platform/desktop/service_port_adapter.hpp"

#include <array>
#include <cstddef>

namespace {

constexpr std::size_t queue_capacity = 8U;

/**
 * @brief Хранит ограниченную FIFO-очередь сервисных кадров.
 */
class FrameQueue {
public:
    /**
     * @brief Добавляет кадр в конец очереди.
     */
    bool push(const drivers::service_port::Frame& frame)
    {
        if (size_ == frames_.size()) {
            return false;
        }
        frames_[tail_] = frame;
        tail_ = (tail_ + 1U) % frames_.size();
        ++size_;
        return true;
    }

    /**
     * @brief Извлекает самый старый кадр.
     */
    bool pop(drivers::service_port::Frame& frame)
    {
        if (size_ == 0U) {
            return false;
        }
        frame = frames_[head_];
        head_ = (head_ + 1U) % frames_.size();
        --size_;
        return true;
    }

    /**
     * @brief Очищает очередь.
     */
    void clear()
    {
        head_ = 0U;
        tail_ = 0U;
        size_ = 0U;
    }

private:
    std::array<drivers::service_port::Frame, queue_capacity> frames_{};
    std::size_t head_{};
    std::size_t tail_{};
    std::size_t size_{};
};

FrameQueue received_frames;
FrameQueue transmitted_frames;

}  // безымянное пространство имён

namespace drivers::service_port {

void reset()
{
    received_frames.clear();
    transmitted_frames.clear();
}

bool receive(Frame& frame)
{
    return received_frames.pop(frame);
}

bool send(const Frame& frame)
{
    return is_valid(frame) && transmitted_frames.push(frame);
}

}  // пространство имён drivers::service_port

namespace platform::desktop_service_port {

bool inject_received(const drivers::service_port::Frame& frame)
{
    return drivers::service_port::is_valid(frame)
        && received_frames.push(frame);
}

bool pop_transmitted(drivers::service_port::Frame& frame)
{
    return transmitted_frames.pop(frame);
}

}  // пространство имён platform::desktop_service_port
