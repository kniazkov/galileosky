/**
 * @file runner.cpp
 * @brief Реализует управляемый JSONL-стенд для desktop-сборки.
 *
 * Процесс читает по одной JSON-команде из stdin, управляет виртуальным временем,
 * вызывает общий tick до устойчивого состояния и пишет события в stdout. Для
 * состояния передаются только новые ревизии; snapshot принудительно возвращает
 * полный текущий снимок.
 */

#include "application/application.hpp"
#include "drivers/can.hpp"
#include "drivers/gnss.hpp"
#include "drivers/service_port.hpp"
#include "drivers/server_transport.hpp"
#include "drivers/sensors.hpp"
#include "modules/power_management.hpp"
#include "modules/server_transmission.hpp"
#include "platform/desktop/can_adapter.hpp"
#include "platform/desktop/gnss_adapter.hpp"
#include "platform/desktop/service_port_adapter.hpp"
#include "platform/desktop/server_transport_adapter.hpp"
#include "platform/desktop/sensors_adapter.hpp"
#include "platform/runtime.hpp"

#include <array>
#include <cctype>
#include <cstdint>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

constexpr std::size_t maximum_ticks_per_command = 1'024U;

/**
 * @brief Перечисляет команды, доступные управляющему сценарию.
 */
enum class Command {
    tick,
    advance,
    can_rx,
    adc_set,
    digital_set,
    gnss_set,
    service_rx,
    server_enqueue,
    server_online,
    power_snapshot,
    snapshot,
    reset,
    shutdown
};

/**
 * @brief Содержит проверенную и готовую к выполнению команду.
 */
struct Request {
    std::uint64_t id;
    Command command;
    std::uint32_t argument;
    drivers::can::Frame can_frame;
    drivers::server_transport::Message server_message;
    bool server_online;
    std::uint8_t sensor_channel;
    std::uint16_t sensor_value;
    bool sensor_active;
    drivers::gnss::Fix gnss_fix;
    drivers::service_port::Frame service_frame;
};

/**
 * @brief Обозначает ошибку входного протокола или выполнения команды.
 */
class ProtocolError final : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

/**
 * @brief Разбирает плоский JSON-объект без внешней библиотеки.
 */
class JsonReader {
public:
    /**
     * @brief Создаёт читатель для одной входной строки.
     */
    explicit JsonReader(const std::string_view input)
        : input_{input}
    {
    }

    /**
     * @brief Требует указанный символ в текущей позиции.
     */
    void expect(const char expected)
    {
        skip_whitespace();
        if (position_ == input_.size() || input_[position_] != expected) {
            throw ProtocolError{"нарушена структура JSON"};
        }
        ++position_;
    }

    /**
     * @brief Считывает указанный символ, если он присутствует.
     * @return true, если символ был считан.
     */
    bool consume(const char value)
    {
        skip_whitespace();
        if (position_ != input_.size() && input_[position_] == value) {
            ++position_;
            return true;
        }
        return false;
    }

    /**
     * @brief Считывает строковое значение JSON.
     */
    std::string read_string()
    {
        skip_whitespace();
        if (position_ == input_.size() || input_[position_] != '"') {
            throw ProtocolError{"ожидалась строка JSON"};
        }
        ++position_;

        std::string result;
        while (position_ != input_.size()) {
            const char value = input_[position_++];
            if (value == '"') {
                return result;
            }
            if (static_cast<unsigned char>(value) < 0x20U) {
                throw ProtocolError{"управляющий символ внутри строки JSON"};
            }
            if (value != '\\') {
                result.push_back(value);
                continue;
            }
            if (position_ == input_.size()) {
                throw ProtocolError{"незавершённая escape-последовательность"};
            }

            const char escaped = input_[position_++];
            switch (escaped) {
            case '"':
            case '\\':
            case '/':
                result.push_back(escaped);
                break;
            case 'b':
                result.push_back('\b');
                break;
            case 'f':
                result.push_back('\f');
                break;
            case 'n':
                result.push_back('\n');
                break;
            case 'r':
                result.push_back('\r');
                break;
            case 't':
                result.push_back('\t');
                break;
            default:
                throw ProtocolError{
                    "поддерживаются только однобайтовые escape-последовательности"};
            }
        }

        throw ProtocolError{"незавершённая строка JSON"};
    }

    /**
     * @brief Считывает беззнаковое целое значение JSON.
     */
    std::uint64_t read_unsigned()
    {
        skip_whitespace();
        if (position_ == input_.size()
            || !std::isdigit(static_cast<unsigned char>(input_[position_]))) {
            throw ProtocolError{"ожидалось беззнаковое целое число"};
        }

        std::uint64_t result = 0U;
        const std::size_t first_digit = position_;
        while (position_ != input_.size()
               && std::isdigit(static_cast<unsigned char>(input_[position_]))) {
            const std::uint64_t digit =
                static_cast<std::uint64_t>(input_[position_] - '0');
            if (result > (std::numeric_limits<std::uint64_t>::max() - digit) / 10U) {
                throw ProtocolError{"целое число не помещается в 64 бита"};
            }
            result = result * 10U + digit;
            ++position_;
        }

        if (position_ - first_digit > 1U && input_[first_digit] == '0') {
            throw ProtocolError{"ведущие нули в числе JSON запрещены"};
        }
        return result;
    }

    /**
     * @brief Считывает целое значение, помещающееся в 32 знаковых бита.
     */
    std::int32_t read_signed_32()
    {
        const bool negative = consume('-');
        if (negative
            && (position_ == input_.size()
                || !std::isdigit(
                    static_cast<unsigned char>(input_[position_])))) {
            throw ProtocolError{"после минуса ожидалась цифра"};
        }
        const std::uint64_t magnitude = read_unsigned();
        const std::uint64_t negative_limit =
            static_cast<std::uint64_t>(
                std::numeric_limits<std::int32_t>::max()) + 1U;

        if ((!negative
                && magnitude
                    > static_cast<std::uint64_t>(
                        std::numeric_limits<std::int32_t>::max()))
            || (negative && magnitude > negative_limit)) {
            throw ProtocolError{"число не помещается в 32 знаковых бита"};
        }

        if (negative && magnitude == negative_limit) {
            return std::numeric_limits<std::int32_t>::min();
        }

        const std::int32_t value = static_cast<std::int32_t>(magnitude);
        return negative ? -value : value;
    }

    /**
     * @brief Считывает логическое значение JSON.
     */
    bool read_boolean()
    {
        skip_whitespace();
        if (input_.substr(position_, 4U) == "true") {
            position_ += 4U;
            return true;
        }
        if (input_.substr(position_, 5U) == "false") {
            position_ += 5U;
            return false;
        }
        throw ProtocolError{"ожидалось логическое значение"};
    }

    /**
     * @brief Считывает массив байтов с ограниченной ёмкостью.
     * @return Количество считанных байтов.
     */
    template<std::size_t capacity>
    std::uint8_t read_byte_array(
        std::array<std::uint8_t, capacity>& values)
    {
        expect('[');
        std::uint8_t length = 0U;
        if (consume(']')) {
            return length;
        }

        for (;;) {
            if (length == values.size()) {
                throw ProtocolError{"массив байтов превышает допустимый размер"};
            }

            const std::uint64_t value = read_unsigned();
            if (value > std::numeric_limits<std::uint8_t>::max()) {
                throw ProtocolError{"значение не помещается в один байт"};
            }
            values[length] = static_cast<std::uint8_t>(value);
            ++length;

            if (consume(']')) {
                return length;
            }
            expect(',');
        }
    }

    /**
     * @brief Проверяет отсутствие данных после разобранного объекта.
     */
    void finish()
    {
        skip_whitespace();
        if (position_ != input_.size()) {
            throw ProtocolError{"после объекта JSON обнаружены лишние данные"};
        }
    }

private:
    /**
     * @brief Пропускает разрешённые JSON пробельные символы.
     */
    void skip_whitespace()
    {
        while (position_ != input_.size()
               && std::isspace(static_cast<unsigned char>(input_[position_]))) {
            ++position_;
        }
    }

    std::string_view input_;
    std::size_t position_{};
};

std::uint32_t narrow_to_u32(const std::uint64_t value)
{
    if (value > std::numeric_limits<std::uint32_t>::max()) {
        throw ProtocolError{"число не помещается в 32 бита"};
    }
    return static_cast<std::uint32_t>(value);
}

Command parse_command(const std::string& value)
{
    if (value == "tick") {
        return Command::tick;
    }
    if (value == "advance") {
        return Command::advance;
    }
    if (value == "can_rx") {
        return Command::can_rx;
    }
    if (value == "adc_set") {
        return Command::adc_set;
    }
    if (value == "digital_set") {
        return Command::digital_set;
    }
    if (value == "gnss_set") {
        return Command::gnss_set;
    }
    if (value == "service_rx") {
        return Command::service_rx;
    }
    if (value == "server_enqueue") {
        return Command::server_enqueue;
    }
    if (value == "server_online") {
        return Command::server_online;
    }
    if (value == "power_snapshot") {
        return Command::power_snapshot;
    }
    if (value == "snapshot") {
        return Command::snapshot;
    }
    if (value == "reset") {
        return Command::reset;
    }
    if (value == "shutdown") {
        return Command::shutdown;
    }
    throw ProtocolError{"неизвестная команда"};
}

Request parse_request(const std::string& line)
{
    JsonReader reader{line};
    reader.expect('{');

    std::uint64_t id = 0U;
    std::uint32_t timestamp_ms = 0U;
    std::uint32_t milliseconds = 0U;
    drivers::can::Frame can_frame{};
    drivers::server_transport::Message server_message{};
    bool server_online = false;
    std::uint8_t sensor_channel = 0U;
    std::uint16_t sensor_value = 0U;
    bool sensor_active = false;
    drivers::gnss::Fix gnss_fix{};
    drivers::service_port::Frame service_frame{};
    std::string command_text;
    bool has_id = false;
    bool has_command = false;
    bool has_timestamp = false;
    bool has_milliseconds = false;
    bool has_can_id = false;
    bool has_can_data = false;
    bool has_message_id = false;
    bool has_payload = false;
    bool has_server_online = false;
    bool has_sensor_channel = false;
    bool has_sensor_value = false;
    bool has_sensor_active = false;
    bool has_latitude = false;
    bool has_longitude = false;
    bool has_ground_speed = false;
    bool has_fix_valid = false;
    bool has_service_data = false;

    if (!reader.consume('}')) {
        for (;;) {
            const std::string key = reader.read_string();
            reader.expect(':');

            if (key == "id") {
                if (has_id) {
                    throw ProtocolError{"поле id указано несколько раз"};
                }
                id = reader.read_unsigned();
                has_id = true;
            } else if (key == "command") {
                if (has_command) {
                    throw ProtocolError{"поле command указано несколько раз"};
                }
                command_text = reader.read_string();
                has_command = true;
            } else if (key == "timestamp_ms") {
                if (has_timestamp) {
                    throw ProtocolError{"поле timestamp_ms указано несколько раз"};
                }
                timestamp_ms = narrow_to_u32(reader.read_unsigned());
                has_timestamp = true;
            } else if (key == "milliseconds") {
                if (has_milliseconds) {
                    throw ProtocolError{"поле milliseconds указано несколько раз"};
                }
                milliseconds = narrow_to_u32(reader.read_unsigned());
                has_milliseconds = true;
            } else if (key == "can_id") {
                if (has_can_id) {
                    throw ProtocolError{"поле can_id указано несколько раз"};
                }
                const std::uint64_t identifier = reader.read_unsigned();
                if (identifier > drivers::can::maximum_identifier) {
                    throw ProtocolError{"can_id должен быть 11-битным"};
                }
                can_frame.identifier =
                    static_cast<std::uint16_t>(identifier);
                has_can_id = true;
            } else if (key == "data") {
                if (has_can_data) {
                    throw ProtocolError{"поле data указано несколько раз"};
                }
                can_frame.length = reader.read_byte_array(can_frame.data);
                has_can_data = true;
            } else if (key == "message_id") {
                if (has_message_id) {
                    throw ProtocolError{"поле message_id указано несколько раз"};
                }
                server_message.message_id =
                    narrow_to_u32(reader.read_unsigned());
                has_message_id = true;
            } else if (key == "payload") {
                if (has_payload) {
                    throw ProtocolError{"поле payload указано несколько раз"};
                }
                server_message.length =
                    reader.read_byte_array(server_message.payload);
                has_payload = true;
            } else if (key == "online") {
                if (has_server_online) {
                    throw ProtocolError{"поле online указано несколько раз"};
                }
                server_online = reader.read_boolean();
                has_server_online = true;
            } else if (key == "channel") {
                if (has_sensor_channel) {
                    throw ProtocolError{"поле channel указано несколько раз"};
                }
                const std::uint64_t channel = reader.read_unsigned();
                if (channel > std::numeric_limits<std::uint8_t>::max()) {
                    throw ProtocolError{"номер канала не помещается в 8 бит"};
                }
                sensor_channel = static_cast<std::uint8_t>(channel);
                has_sensor_channel = true;
            } else if (key == "adc_value") {
                if (has_sensor_value) {
                    throw ProtocolError{"поле adc_value указано несколько раз"};
                }
                const std::uint64_t value = reader.read_unsigned();
                if (value > drivers::sensors::maximum_adc_value) {
                    throw ProtocolError{"adc_value должен быть от 0 до 4095"};
                }
                sensor_value = static_cast<std::uint16_t>(value);
                has_sensor_value = true;
            } else if (key == "active") {
                if (has_sensor_active) {
                    throw ProtocolError{"поле active указано несколько раз"};
                }
                sensor_active = reader.read_boolean();
                has_sensor_active = true;
            } else if (key == "latitude_e7") {
                if (has_latitude) {
                    throw ProtocolError{
                        "поле latitude_e7 указано несколько раз"};
                }
                gnss_fix.latitude_e7 = reader.read_signed_32();
                has_latitude = true;
            } else if (key == "longitude_e7") {
                if (has_longitude) {
                    throw ProtocolError{
                        "поле longitude_e7 указано несколько раз"};
                }
                gnss_fix.longitude_e7 = reader.read_signed_32();
                has_longitude = true;
            } else if (key == "ground_speed_centi_kph") {
                if (has_ground_speed) {
                    throw ProtocolError{
                        "поле ground_speed_centi_kph указано несколько раз"};
                }
                const std::uint64_t speed = reader.read_unsigned();
                if (speed > std::numeric_limits<std::uint16_t>::max()) {
                    throw ProtocolError{
                        "ground_speed_centi_kph не помещается в 16 бит"};
                }
                gnss_fix.ground_speed_centi_kph =
                    static_cast<std::uint16_t>(speed);
                has_ground_speed = true;
            } else if (key == "valid") {
                if (has_fix_valid) {
                    throw ProtocolError{"поле valid указано несколько раз"};
                }
                gnss_fix.valid = reader.read_boolean();
                has_fix_valid = true;
            } else if (key == "service_data") {
                if (has_service_data) {
                    throw ProtocolError{
                        "поле service_data указано несколько раз"};
                }
                service_frame.length =
                    reader.read_byte_array(service_frame.data);
                has_service_data = true;
            } else {
                throw ProtocolError{"неизвестное поле команды"};
            }

            if (reader.consume('}')) {
                break;
            }
            reader.expect(',');
        }
    }
    reader.finish();

    if (!has_id || !has_command) {
        throw ProtocolError{"обязательны поля id и command"};
    }

    const Command command = parse_command(command_text);
    const bool has_server_message = has_message_id || has_payload;
    const bool has_sensor_fields =
        has_sensor_channel || has_sensor_value || has_sensor_active;
    const bool has_gnss_fields =
        has_latitude || has_longitude || has_ground_speed || has_fix_valid;
    switch (command) {
    case Command::tick:
        if (!has_timestamp || has_milliseconds || has_can_id || has_can_data
            || has_server_message || has_server_online || has_sensor_fields
            || has_gnss_fields || has_service_data) {
            throw ProtocolError{"команде tick требуется только timestamp_ms"};
        }
        return Request{
            id, command, timestamp_ms, {}, {}, false, 0U, 0U, false, {}, {}};
    case Command::advance:
        if (!has_milliseconds || has_timestamp || has_can_id || has_can_data
            || has_server_message || has_server_online || has_sensor_fields
            || has_gnss_fields || has_service_data) {
            throw ProtocolError{"команде advance требуется только milliseconds"};
        }
        return Request{
            id, command, milliseconds, {}, {}, false, 0U, 0U, false, {}, {}};
    case Command::can_rx:
        if (has_timestamp || has_milliseconds || !has_can_id || !has_can_data
            || has_server_message || has_server_online || has_sensor_fields
            || has_gnss_fields || has_service_data) {
            throw ProtocolError{"команде can_rx требуются can_id и data"};
        }
        return Request{
            id, command, 0U, can_frame, {}, false, 0U, 0U, false, {}, {}};
    case Command::adc_set:
        if (has_timestamp || has_milliseconds || has_can_id || has_can_data
            || has_server_message || has_server_online
            || !has_sensor_channel || !has_sensor_value || has_sensor_active
            || sensor_channel > 1U || has_gnss_fields || has_service_data) {
            throw ProtocolError{
                "команде adc_set требуются channel 0..1 и adc_value"};
        }
        return Request{
            id,
            command,
            0U,
            {},
            {},
            false,
            sensor_channel,
            sensor_value,
            false,
            {},
            {}};
    case Command::digital_set:
        if (has_timestamp || has_milliseconds || has_can_id || has_can_data
            || has_server_message || has_server_online
            || !has_sensor_channel || has_sensor_value || !has_sensor_active
            || sensor_channel > 2U || has_gnss_fields || has_service_data) {
            throw ProtocolError{
                "команде digital_set требуются channel 0..2 и active"};
        }
        return Request{
            id,
            command,
            0U,
            {},
            {},
            false,
            sensor_channel,
            0U,
            sensor_active,
            {},
            {}};
    case Command::gnss_set:
        if (has_timestamp || has_milliseconds || has_can_id || has_can_data
            || has_server_message || has_server_online || has_sensor_fields
            || has_service_data || !has_fix_valid
            || (gnss_fix.valid
                && (!has_latitude || !has_longitude || !has_ground_speed))
            || (!gnss_fix.valid
                && (has_latitude || has_longitude || has_ground_speed))
            || !drivers::gnss::is_valid(gnss_fix)) {
            throw ProtocolError{
                "gnss_set требует valid и, для valid=true, координаты и скорость"};
        }
        return Request{
            id,
            command,
            0U,
            {},
            {},
            false,
            0U,
            0U,
            false,
            gnss_fix,
            {}};
    case Command::service_rx:
        if (has_timestamp || has_milliseconds || has_can_id || has_can_data
            || has_server_message || has_server_online || has_sensor_fields
            || has_gnss_fields || !has_service_data) {
            throw ProtocolError{
                "команде service_rx требуется только service_data"};
        }
        return Request{
            id,
            command,
            0U,
            {},
            {},
            false,
            0U,
            0U,
            false,
            {},
            service_frame};
    case Command::server_enqueue:
        if (has_timestamp || has_milliseconds || has_can_id || has_can_data
            || !has_message_id || !has_payload || has_server_online
            || has_sensor_fields || has_gnss_fields || has_service_data) {
            throw ProtocolError{
                "команде server_enqueue требуются message_id и payload"};
        }
        return Request{
            id,
            command,
            0U,
            {},
            server_message,
            false,
            0U,
            0U,
            false,
            {},
            {}};
    case Command::server_online:
        if (has_timestamp || has_milliseconds || has_can_id || has_can_data
            || has_server_message || !has_server_online || has_sensor_fields
            || has_gnss_fields || has_service_data) {
            throw ProtocolError{"команде server_online требуется поле online"};
        }
        return Request{
            id,
            command,
            0U,
            {},
            {},
            server_online,
            0U,
            0U,
            false,
            {},
            {}};
    case Command::power_snapshot:
    case Command::snapshot:
    case Command::reset:
    case Command::shutdown:
        if (has_timestamp || has_milliseconds || has_can_id || has_can_data
            || has_server_message || has_server_online || has_sensor_fields
            || has_gnss_fields || has_service_data) {
            throw ProtocolError{"команда не принимает дополнительные поля"};
        }
        return Request{
            id, command, 0U, {}, {}, false, 0U, 0U, false, {}, {}};
    }

    throw ProtocolError{"невозможное значение команды"};
}

std::string escape_json(const std::string_view value)
{
    std::string result;
    result.reserve(value.size());

    for (const unsigned char symbol : value) {
        switch (symbol) {
        case '"':
            result += "\\\"";
            break;
        case '\\':
            result += "\\\\";
            break;
        case '\b':
            result += "\\b";
            break;
        case '\f':
            result += "\\f";
            break;
        case '\n':
            result += "\\n";
            break;
        case '\r':
            result += "\\r";
            break;
        case '\t':
            result += "\\t";
            break;
        default:
            result.push_back(static_cast<char>(symbol));
            break;
        }
    }
    return result;
}

void run_until_stable(const std::uint32_t timestamp_ms)
{
    for (std::size_t iteration = 0U;
         iteration < maximum_ticks_per_command;
         ++iteration) {
        if (!application::tick(timestamp_ms).work_pending) {
            return;
        }
    }

    throw ProtocolError{"tick не достиг устойчивого состояния"};
}

/**
 * @brief Выдаёт только новые ревизии состояния или полный снимок.
 */
class Reporter {
public:
    /**
     * @brief Отправляет состояние, если оно изменилось или запрошено принудительно.
     */
    void send_state(const std::uint64_t id, const bool force_full)
    {
        const application::StateSnapshot state = application::snapshot();
        const bool application_changed =
            !reported_ || state.revision != last_revision_;
        const bool vehicle_changed =
            state.vehicle.revision != last_vehicle_revision_;
        const bool server_changed =
            state.server_transmission.revision != last_server_revision_;
        const bool sensors_changed =
            state.sensors.revision != last_sensors_revision_;
        const bool navigation_changed =
            state.navigation.revision != last_navigation_revision_;
        const bool geofences_changed =
            state.geofences.revision != last_geofences_revision_;
        if (!force_full
            && !application_changed
            && !vehicle_changed
            && !server_changed
            && !sensors_changed
            && !navigation_changed
            && !geofences_changed) {
            return;
        }

        std::cout
            << "{\"id\":" << id
            << ",\"type\":\"state\",\"full\":"
            << (force_full ? "true" : "false")
            << ",\"state\":{";

        bool field_written = false;
        if (force_full || application_changed) {
            std::cout
                << "\"timestamp_ms\":" << state.timestamp_ms
                << ",\"revision\":" << state.revision;
            field_written = true;
        }

        if (force_full || vehicle_changed) {
            if (field_written) {
                std::cout << ',';
            }
            std::cout
                << "\"vehicle\":{"
                << "\"engine_speed_rpm\":"
                << state.vehicle.engine_speed_rpm
                << ",\"vehicle_speed_kmh\":"
                << static_cast<unsigned int>(state.vehicle.vehicle_speed_kmh)
                << ",\"coolant_temperature_c\":"
                << state.vehicle.coolant_temperature_c
                << ",\"fuel_level_percent\":"
                << static_cast<unsigned int>(state.vehicle.fuel_level_percent)
                << ",\"check_engine\":"
                << (state.vehicle.check_engine ? "true" : "false")
                << ",\"valid_mask\":"
                << static_cast<unsigned int>(state.vehicle.valid_mask)
                << ",\"revision\":" << state.vehicle.revision
                << '}';
            field_written = true;
        }

        if (force_full || sensors_changed) {
            if (field_written) {
                std::cout << ',';
            }
            std::cout
                << "\"sensors\":{"
                << "\"supply_voltage_adc\":"
                << state.sensors.supply_voltage_adc
                << ",\"external_input_adc\":"
                << state.sensors.external_input_adc
                << ",\"ignition\":"
                << (state.sensors.ignition ? "true" : "false")
                << ",\"door_open\":"
                << (state.sensors.door_open ? "true" : "false")
                << ",\"alarm\":"
                << (state.sensors.alarm ? "true" : "false")
                << ",\"valid_mask\":"
                << static_cast<unsigned int>(state.sensors.valid_mask)
                << ",\"revision\":" << state.sensors.revision
                << '}';
            field_written = true;
        }

        if (force_full || navigation_changed) {
            if (field_written) {
                std::cout << ',';
            }
            std::cout
                << "\"navigation\":{"
                << "\"latitude_e7\":"
                << state.navigation.latitude_e7
                << ",\"longitude_e7\":"
                << state.navigation.longitude_e7
                << ",\"ground_speed_centi_kph\":"
                << state.navigation.ground_speed_centi_kph
                << ",\"fix_valid\":"
                << (state.navigation.fix_valid ? "true" : "false")
                << ",\"revision\":" << state.navigation.revision
                << '}';
            field_written = true;
        }

        if (force_full || geofences_changed) {
            if (field_written) {
                std::cout << ',';
            }
            std::cout
                << "\"geofences\":{"
                << "\"inside_mask\":"
                << static_cast<unsigned int>(state.geofences.inside_mask)
                << ",\"position_valid\":"
                << (state.geofences.position_valid ? "true" : "false")
                << ",\"revision\":" << state.geofences.revision
                << '}';
            field_written = true;
        }

        if (force_full || server_changed) {
            if (field_written) {
                std::cout << ',';
            }
            std::cout
                << "\"server_transmission\":{"
                << "\"queued_messages\":"
                << static_cast<unsigned int>(
                    state.server_transmission.queued_messages)
                << ",\"dropped_messages\":"
                << state.server_transmission.dropped_messages
                << ",\"sent_messages\":"
                << state.server_transmission.sent_messages
                << ",\"revision\":"
                << state.server_transmission.revision
                << '}';
        }

        std::cout << "}}" << std::endl;

        last_revision_ = state.revision;
        last_vehicle_revision_ = state.vehicle.revision;
        last_server_revision_ = state.server_transmission.revision;
        last_sensors_revision_ = state.sensors.revision;
        last_navigation_revision_ = state.navigation.revision;
        last_geofences_revision_ = state.geofences.revision;
        reported_ = true;
    }

    /**
     * @brief Заставляет следующий отчёт считаться первым.
     */
    void invalidate()
    {
        reported_ = false;
        last_vehicle_revision_ = 0U;
        last_server_revision_ = 0U;
        last_sensors_revision_ = 0U;
        last_navigation_revision_ = 0U;
        last_geofences_revision_ = 0U;
    }

private:
    std::uint32_t last_revision_{};
    std::uint32_t last_vehicle_revision_{};
    std::uint32_t last_server_revision_{};
    std::uint32_t last_sensors_revision_{};
    std::uint32_t last_navigation_revision_{};
    std::uint32_t last_geofences_revision_{};
    bool reported_{};
};

void send_can_frames(const std::uint64_t id)
{
    drivers::can::Frame frame{};
    while (platform::desktop_can::pop_transmitted(frame)) {
        std::cout
            << "{\"id\":" << id
            << ",\"type\":\"can_tx\",\"can_id\":"
            << frame.identifier
            << ",\"data\":[";

        for (std::uint8_t index = 0U; index < frame.length; ++index) {
            if (index != 0U) {
                std::cout << ',';
            }
            std::cout << static_cast<unsigned int>(frame.data[index]);
        }
        std::cout << "]}" << std::endl;
    }
}

void send_server_messages(const std::uint64_t id)
{
    drivers::server_transport::Message message{};
    while (platform::desktop_server_transport::pop_transmitted(message)) {
        std::cout
            << "{\"id\":" << id
            << ",\"type\":\"server_tx\",\"message_id\":"
            << message.message_id
            << ",\"payload\":[";

        for (std::uint8_t index = 0U; index < message.length; ++index) {
            if (index != 0U) {
                std::cout << ',';
            }
            std::cout << static_cast<unsigned int>(message.payload[index]);
        }
        std::cout << "]}" << std::endl;
    }
}

void send_service_frames(const std::uint64_t id)
{
    drivers::service_port::Frame frame{};
    while (platform::desktop_service_port::pop_transmitted(frame)) {
        std::cout
            << "{\"id\":" << id
            << ",\"type\":\"service_tx\",\"data\":[";

        for (std::uint8_t index = 0U; index < frame.length; ++index) {
            if (index != 0U) {
                std::cout << ',';
            }
            std::cout << static_cast<unsigned int>(frame.data[index]);
        }
        std::cout << "]}" << std::endl;
    }
}

/**
 * @brief Возвращает стабильное имя режима для JSON-протокола.
 */
const char* power_mode_name(const modules::power_management::Mode mode)
{
    using modules::power_management::Mode;

    switch (mode) {
    case Mode::engine_running:
        return "engine_running";
    case Mode::periodic_fix:
        return "periodic_fix";
    case Mode::sleeping:
        return "sleeping";
    }

    return "unknown";
}

/**
 * @brief Выдаёт полный снимок политики питания отдельным сообщением.
 */
void send_power_state(const std::uint64_t id)
{
    const modules::power_management::Snapshot power =
        modules::power_management::snapshot();
    std::cout
        << "{\"id\":" << id
        << ",\"type\":\"power_state\",\"gnss_enabled\":"
        << (power.gnss_enabled ? "true" : "false")
        << ",\"engine_running\":"
        << (power.engine_running ? "true" : "false")
        << ",\"mode\":\"" << power_mode_name(power.mode)
        << "\",\"transitions\":" << power.transitions
        << ",\"revision\":" << power.revision
        << '}' << std::endl;
}

void send_done(const std::uint64_t id)
{
    std::cout
        << "{\"id\":" << id << ",\"type\":\"done\"}"
        << std::endl;
}

void send_error(
    const std::optional<std::uint64_t> id,
    const std::string_view message)
{
    std::cout << "{\"id\":";
    if (id.has_value()) {
        std::cout << *id;
    } else {
        std::cout << "null";
    }
    std::cout
        << ",\"type\":\"error\",\"message\":\""
        << escape_json(message)
        << "\"}" << std::endl;
}

}  // безымянное пространство имён

namespace platform {

int run_application()
{
    std::uint32_t virtual_timestamp_ms = 0U;
    Reporter reporter;
    std::string line;

    while (std::getline(std::cin, line)) {
        std::optional<std::uint64_t> request_id;
        try {
            const Request request = parse_request(line);
            request_id = request.id;

            switch (request.command) {
            case Command::tick:
                virtual_timestamp_ms = request.argument;
                run_until_stable(virtual_timestamp_ms);
                reporter.send_state(request.id, false);
                send_can_frames(request.id);
                send_server_messages(request.id);
                send_done(request.id);
                break;
            case Command::advance:
                virtual_timestamp_ms += request.argument;
                run_until_stable(virtual_timestamp_ms);
                reporter.send_state(request.id, false);
                send_can_frames(request.id);
                send_server_messages(request.id);
                send_done(request.id);
                break;
            case Command::can_rx:
                if (!desktop_can::inject_received(request.can_frame)) {
                    throw ProtocolError{"CAN-драйвер отклонил входящий кадр"};
                }
                run_until_stable(virtual_timestamp_ms);
                reporter.send_state(request.id, false);
                send_can_frames(request.id);
                send_server_messages(request.id);
                send_done(request.id);
                break;
            case Command::adc_set:
                if (!desktop_sensors::set_analog(
                        static_cast<drivers::sensors::AnalogChannel>(
                            request.sensor_channel),
                        request.sensor_value)) {
                    throw ProtocolError{"ADC-драйвер отклонил значение"};
                }
                run_until_stable(virtual_timestamp_ms);
                reporter.send_state(request.id, false);
                send_can_frames(request.id);
                send_server_messages(request.id);
                send_done(request.id);
                break;
            case Command::digital_set:
                desktop_sensors::set_digital(
                    static_cast<drivers::sensors::DigitalInput>(
                        request.sensor_channel),
                    request.sensor_active);
                run_until_stable(virtual_timestamp_ms);
                reporter.send_state(request.id, false);
                send_can_frames(request.id);
                send_server_messages(request.id);
                send_done(request.id);
                break;
            case Command::gnss_set:
                if (!desktop_gnss::set_fix(request.gnss_fix)) {
                    throw ProtocolError{
                        "GPS/ГЛОНАСС-драйвер отклонил фиксацию"};
                }
                run_until_stable(virtual_timestamp_ms);
                reporter.send_state(request.id, false);
                send_can_frames(request.id);
                send_server_messages(request.id);
                send_done(request.id);
                break;
            case Command::service_rx:
                if (!desktop_service_port::inject_received(
                        request.service_frame)) {
                    throw ProtocolError{
                        "сервисный драйвер отклонил входящий кадр"};
                }
                run_until_stable(virtual_timestamp_ms);
                reporter.send_state(request.id, false);
                send_can_frames(request.id);
                send_server_messages(request.id);
                send_service_frames(request.id);
                send_done(request.id);
                break;
            case Command::server_enqueue:
                if (!modules::server_transmission::enqueue(
                        request.server_message)) {
                    throw ProtocolError{"серверное сообщение отклонено"};
                }
                run_until_stable(virtual_timestamp_ms);
                reporter.send_state(request.id, false);
                send_server_messages(request.id);
                send_done(request.id);
                break;
            case Command::server_online:
                desktop_server_transport::set_available(request.server_online);
                run_until_stable(virtual_timestamp_ms);
                reporter.send_state(request.id, false);
                send_server_messages(request.id);
                send_done(request.id);
                break;
            case Command::power_snapshot:
                send_power_state(request.id);
                send_done(request.id);
                break;
            case Command::snapshot:
                reporter.send_state(request.id, true);
                send_done(request.id);
                break;
            case Command::reset:
                application::reset();
                virtual_timestamp_ms = 0U;
                reporter.invalidate();
                reporter.send_state(request.id, true);
                send_done(request.id);
                break;
            case Command::shutdown:
                send_done(request.id);
                return 0;
            }
        } catch (const std::exception& error) {
            send_error(request_id, error.what());
        }
    }

    return 0;
}

}  // пространство имён platform
