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
#include "platform/desktop/can_adapter.hpp"
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
     * @brief Считывает массив байтов полезной нагрузки CAN.
     * @return Количество считанных байтов.
     */
    std::uint8_t read_byte_array(
        std::array<std::uint8_t, drivers::can::maximum_data_length>& values)
    {
        expect('[');
        std::uint8_t length = 0U;
        if (consume(']')) {
            return length;
        }

        for (;;) {
            if (length == values.size()) {
                throw ProtocolError{"полезная нагрузка CAN длиннее восьми байт"};
            }

            const std::uint64_t value = read_unsigned();
            if (value > std::numeric_limits<std::uint8_t>::max()) {
                throw ProtocolError{"байт CAN не помещается в 8 бит"};
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
    std::string command_text;
    bool has_id = false;
    bool has_command = false;
    bool has_timestamp = false;
    bool has_milliseconds = false;
    bool has_can_id = false;
    bool has_can_data = false;

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
    switch (command) {
    case Command::tick:
        if (!has_timestamp || has_milliseconds || has_can_id || has_can_data) {
            throw ProtocolError{"команде tick требуется только timestamp_ms"};
        }
        return Request{id, command, timestamp_ms, {}};
    case Command::advance:
        if (!has_milliseconds || has_timestamp || has_can_id || has_can_data) {
            throw ProtocolError{"команде advance требуется только milliseconds"};
        }
        return Request{id, command, milliseconds, {}};
    case Command::can_rx:
        if (has_timestamp || has_milliseconds || !has_can_id || !has_can_data) {
            throw ProtocolError{"команде can_rx требуются can_id и data"};
        }
        return Request{id, command, 0U, can_frame};
    case Command::snapshot:
    case Command::reset:
    case Command::shutdown:
        if (has_timestamp || has_milliseconds || has_can_id || has_can_data) {
            throw ProtocolError{"команда не принимает числовой аргумент"};
        }
        return Request{id, command, 0U, {}};
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
        if (!force_full && !application_changed && !vehicle_changed) {
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
        }

        std::cout << "}}" << std::endl;

        last_revision_ = state.revision;
        last_vehicle_revision_ = state.vehicle.revision;
        reported_ = true;
    }

    /**
     * @brief Заставляет следующий отчёт считаться первым.
     */
    void invalidate()
    {
        reported_ = false;
        last_vehicle_revision_ = 0U;
    }

private:
    std::uint32_t last_revision_{};
    std::uint32_t last_vehicle_revision_{};
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
                send_done(request.id);
                break;
            case Command::advance:
                virtual_timestamp_ms += request.argument;
                run_until_stable(virtual_timestamp_ms);
                reporter.send_state(request.id, false);
                send_can_frames(request.id);
                send_done(request.id);
                break;
            case Command::can_rx:
                if (!desktop_can::inject_received(request.can_frame)) {
                    throw ProtocolError{"CAN-драйвер отклонил входящий кадр"};
                }
                run_until_stable(virtual_timestamp_ms);
                reporter.send_state(request.id, false);
                send_can_frames(request.id);
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
