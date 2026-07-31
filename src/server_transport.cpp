/**
 * @file server_transport.cpp
 * @brief Реализует общую проверку сообщения серверного транспорта.
 */

#include "drivers/server_transport.hpp"

namespace drivers::server_transport {

bool is_valid(const Message& message)
{
    return message.length <= maximum_payload_size;
}

}  // пространство имён drivers::server_transport
