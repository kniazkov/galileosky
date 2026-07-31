/**
 * @file server_transport.cpp
 * @brief Реализует границу серверного транспорта для целевой сборки.
 *
 * Канал связи в задании не определён, поэтому заглушка не подтверждает
 * доступность сети и не принимает сообщения к отправке.
 */

#include "drivers/server_transport.hpp"

namespace drivers::server_transport {

void reset()
{
}

bool available()
{
    return false;
}

bool send(const Message& message)
{
    (void)message;
    return false;
}

}  // пространство имён drivers::server_transport
