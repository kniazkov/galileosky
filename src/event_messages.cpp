/**
 * @file event_messages.cpp
 * @brief Реализует общую нумерацию и упаковку прикладных событий.
 */

#include "modules/event_messages.hpp"

#include "drivers/server_transport.hpp"
#include "modules/server_transmission.hpp"

namespace {

std::uint32_t next_message_id = 1U;

}  // безымянное пространство имён

namespace modules::event_messages {

void reset()
{
    next_message_id = 1U;
}

bool enqueue(const std::uint8_t* const payload, const std::uint8_t length)
{
    if (length == 0U
        || length > drivers::server_transport::maximum_payload_size
        || payload == nullptr) {
        return false;
    }

    drivers::server_transport::Message message{};
    message.message_id = next_message_id;
    message.length = length;
    for (std::uint8_t index = 0U; index < length; ++index) {
        message.payload[index] = payload[index];
    }

    if (!modules::server_transmission::enqueue(message)) {
        return false;
    }

    ++next_message_id;
    return true;
}

}  // пространство имён modules::event_messages
