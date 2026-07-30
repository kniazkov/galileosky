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
#include "platform/runtime.hpp"

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
    std::string command_text;
    bool has_id = false;
    bool has_command = false;
    bool has_timestamp = false;
    bool has_milliseconds = false;

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
        if (!has_timestamp || has_milliseconds) {
            throw ProtocolError{"команде tick требуется только timestamp_ms"};
        }
        return Request{id, command, timestamp_ms};
    case Command::advance:
        if (!has_milliseconds || has_timestamp) {
            throw ProtocolError{"команде advance требуется только milliseconds"};
        }
        return Request{id, command, milliseconds};
    case Command::snapshot:
    case Command::reset:
    case Command::shutdown:
        if (has_timestamp || has_milliseconds) {
            throw ProtocolError{"команда не принимает числовой аргумент"};
        }
        return Request{id, command, 0U};
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
        if (!force_full && reported_ && state.revision == last_revision_) {
            return;
        }

        std::cout
            << "{\"id\":" << id
            << ",\"type\":\"state\",\"full\":"
            << (force_full ? "true" : "false")
            << ",\"state\":{\"timestamp_ms\":" << state.timestamp_ms
            << ",\"revision\":" << state.revision
            << "}}" << std::endl;

        last_revision_ = state.revision;
        reported_ = true;
    }

    /**
     * @brief Заставляет следующий отчёт считаться первым.
     */
    void invalidate()
    {
        reported_ = false;
    }

private:
    std::uint32_t last_revision_{};
    bool reported_{};
};

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
                send_done(request.id);
                break;
            case Command::advance:
                virtual_timestamp_ms += request.argument;
                run_until_stable(virtual_timestamp_ms);
                reporter.send_state(request.id, false);
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
