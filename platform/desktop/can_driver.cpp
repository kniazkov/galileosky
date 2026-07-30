/**
 * @file can_driver.cpp
 * @brief Реализует CAN-драйвер desktop-стенда на статических очередях.
 *
 * Входная очередь заполняется JSONL-адаптером, а выходная сохраняет кадры
 * приложения до их выдачи в stdout.
 */

#include "drivers/can.hpp"
#include "platform/desktop/can_adapter.hpp"

#include <array>
#include <cstddef>

namespace {

constexpr std::size_t queue_capacity = 16U;

/**
 * @brief Хранит ограниченную FIFO-очередь кадров без динамической памяти.
 */
class FrameQueue {
public:
    /**
     * @brief Добавляет кадр в конец очереди.
     * @return false, если очередь заполнена.
     */
    bool push(const drivers::can::Frame& frame)
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
     * @return false, если очередь пуста.
     */
    bool pop(drivers::can::Frame& frame)
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
     * @brief Удаляет все накопленные кадры.
     */
    void clear()
    {
        head_ = 0U;
        tail_ = 0U;
        size_ = 0U;
    }

private:
    std::array<drivers::can::Frame, queue_capacity> frames_{};
    std::size_t head_{};
    std::size_t tail_{};
    std::size_t size_{};
};

FrameQueue received_frames;
FrameQueue transmitted_frames;

}  // безымянное пространство имён

namespace drivers::can {

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

}  // пространство имён drivers::can

namespace platform::desktop_can {

bool inject_received(const drivers::can::Frame& frame)
{
    return drivers::can::is_valid(frame) && received_frames.push(frame);
}

bool pop_transmitted(drivers::can::Frame& frame)
{
    return transmitted_frames.pop(frame);
}

}  // пространство имён platform::desktop_can
