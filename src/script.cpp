/**
 * @file script.cpp
 * @brief Реализует небольшой BASIC без циклов и динамической памяти.
 *
 * Исходник разбирается заново при каждом запуске. Это экономит RAM на
 * синтаксическом дереве, а отсутствие циклов ограничивает время выполнения.
 */

#include "modules/script.hpp"

#include "modules/configuration.hpp"
#include "modules/diagnostics.hpp"
#include "modules/event_messages.hpp"
#include "modules/sensors.hpp"
#include "modules/vehicle.hpp"

#include <cstddef>
#include <cstdint>

namespace {

constexpr float maximum_absolute_value = 1'000'000'000.0F;
constexpr std::uint8_t script_message_code = 0x20U;

constexpr char default_program[] =
    "IF SPEED > 130 THEN\n"
    "  IF ALERT = 0 THEN\n"
    "    SEND(SPEED)\n"
    "    ALERT = 1\n"
    "  END IF\n"
    "ELSE\n"
    "  ALERT = 0\n"
    "END IF";

/**
 * @brief Хранит одну пользовательскую переменную BASIC.
 */
struct Variable {
    char name[modules::script::maximum_name_length + 1U]{};
    float value{};
};

/**
 * @brief Запоминает версии данных, доступных встроенным переменным.
 */
struct SourceVersions {
    std::uint32_t timestamp_ms{};
    std::uint32_t vehicle{};
    std::uint32_t sensors{};
    std::uint32_t configuration{};
    bool captured{};
};

/**
 * @brief Хранит программу, переменные и наблюдаемое состояние интерпретатора.
 */
struct ScriptState {
    char program[modules::script::maximum_program_size + 1U]{};
    Variable variables[modules::script::maximum_variables]{};
    modules::script::Snapshot snapshot{};
    SourceVersions sources{};
};

ScriptState state{};

/**
 * @brief Описывает один активный уровень условного блока.
 */
struct BranchFrame {
    bool parent_active{};
    bool condition_true{};
    bool active{};
    bool else_seen{};
};

enum class Builtin : std::uint8_t {
    none,
    time,
    speed,
    rpm,
    ignition,
    geofence,
    logcode
};

char upper_ascii(const char value)
{
    if (value >= 'a' && value <= 'z') {
        return static_cast<char>(value - 'a' + 'A');
    }
    return value;
}

bool is_letter(const char value)
{
    const char upper = upper_ascii(value);
    return upper >= 'A' && upper <= 'Z';
}

bool is_digit(const char value)
{
    return value >= '0' && value <= '9';
}

bool is_identifier_start(const char value)
{
    return is_letter(value) || value == '_';
}

bool is_identifier_part(const char value)
{
    return is_identifier_start(value) || is_digit(value);
}

bool names_equal(const char* left, const char* right)
{
    std::size_t index = 0U;
    while (left[index] != '\0' && right[index] != '\0') {
        if (left[index] != right[index]) {
            return false;
        }
        ++index;
    }
    return left[index] == right[index];
}

Builtin find_builtin(const char* name)
{
    if (names_equal(name, "TIME")) {
        return Builtin::time;
    }
    if (names_equal(name, "SPEED")) {
        return Builtin::speed;
    }
    if (names_equal(name, "RPM")) {
        return Builtin::rpm;
    }
    if (names_equal(name, "IGNITION")) {
        return Builtin::ignition;
    }
    if (names_equal(name, "GEOFENCE")) {
        return Builtin::geofence;
    }
    if (names_equal(name, "LOGCODE")) {
        return Builtin::logcode;
    }
    return Builtin::none;
}

void set_error(
    const modules::script::Error error,
    const std::uint16_t line)
{
    if (state.snapshot.error != modules::script::Error::none) {
        return;
    }
    state.snapshot.faulted = true;
    state.snapshot.error = error;
    state.snapshot.error_line = line;
    ++state.snapshot.revision;
}

bool value_is_valid(const float value)
{
    return value >= -maximum_absolute_value
        && value <= maximum_absolute_value;
}

Variable* find_variable(
    const char* name,
    const bool create,
    const std::uint16_t line)
{
    for (std::uint8_t index = 0U;
         index < state.snapshot.variables;
         ++index) {
        if (names_equal(state.variables[index].name, name)) {
            return &state.variables[index];
        }
    }

    if (!create) {
        set_error(modules::script::Error::syntax, line);
        return nullptr;
    }
    if (state.snapshot.variables == modules::script::maximum_variables) {
        set_error(modules::script::Error::variable_limit, line);
        return nullptr;
    }

    Variable& variable = state.variables[state.snapshot.variables];
    std::size_t index = 0U;
    do {
        variable.name[index] = name[index];
        ++index;
    } while (name[index - 1U] != '\0');
    ++state.snapshot.variables;
    return &variable;
}

bool read_builtin(
    const Builtin builtin,
    const std::uint32_t timestamp_ms,
    float& value,
    const std::uint16_t line)
{
    switch (builtin) {
    case Builtin::time:
        value = static_cast<float>(timestamp_ms);
        return true;
    case Builtin::speed: {
        const modules::vehicle::Snapshot vehicle =
            modules::vehicle::snapshot();
        value =
            (vehicle.valid_mask & modules::vehicle::vehicle_speed_valid) != 0U
                ? static_cast<float>(vehicle.vehicle_speed_kmh)
                : 0.0F;
        return true;
    }
    case Builtin::rpm: {
        const modules::vehicle::Snapshot vehicle =
            modules::vehicle::snapshot();
        value =
            (vehicle.valid_mask & modules::vehicle::engine_speed_valid) != 0U
                ? static_cast<float>(vehicle.engine_speed_rpm)
                : 0.0F;
        return true;
    }
    case Builtin::ignition: {
        const modules::sensors::Snapshot sensors =
            modules::sensors::snapshot();
        value =
            (sensors.valid_mask & modules::sensors::ignition_valid) != 0U
                && sensors.ignition
                ? 1.0F
                : 0.0F;
        return true;
    }
    case Builtin::geofence:
        value = modules::configuration::snapshot().geofences_enabled
            ? 1.0F
            : 0.0F;
        return true;
    case Builtin::logcode:
        set_error(modules::script::Error::access_denied, line);
        return false;
    case Builtin::none:
        break;
    }
    set_error(modules::script::Error::syntax, line);
    return false;
}

bool write_builtin(
    const Builtin builtin,
    const float value,
    const std::uint32_t timestamp_ms,
    const bool apply,
    const std::uint16_t line)
{
    switch (builtin) {
    case Builtin::geofence:
        if (apply) {
            (void)modules::configuration::set_geofences_enabled(
                timestamp_ms,
                value != 0.0F);
        }
        return true;
    case Builtin::logcode:
        if (value < 0.0F || value > maximum_absolute_value) {
            set_error(modules::script::Error::numeric_range, line);
            return false;
        }
        if (apply) {
            modules::diagnostics::record_script_value(
                timestamp_ms,
                static_cast<std::uint32_t>(value));
        }
        return true;
    case Builtin::time:
    case Builtin::speed:
    case Builtin::rpm:
    case Builtin::ignition:
        set_error(modules::script::Error::access_denied, line);
        return false;
    case Builtin::none:
        break;
    }
    set_error(modules::script::Error::syntax, line);
    return false;
}

bool read_value(
    const char* name,
    const std::uint32_t timestamp_ms,
    const bool evaluate,
    float& value,
    const std::uint16_t line)
{
    const Builtin builtin = find_builtin(name);
    if (builtin != Builtin::none) {
        if (!evaluate) {
            if (builtin == Builtin::logcode) {
                set_error(modules::script::Error::access_denied, line);
                return false;
            }
            value = 0.0F;
            return true;
        }
        return read_builtin(builtin, timestamp_ms, value, line);
    }

    Variable* const variable = find_variable(name, true, line);
    if (variable == nullptr) {
        return false;
    }
    value = evaluate ? variable->value : 0.0F;
    return true;
}

bool write_value(
    const char* name,
    const float value,
    const std::uint32_t timestamp_ms,
    const bool apply,
    const std::uint16_t line)
{
    const Builtin builtin = find_builtin(name);
    if (builtin != Builtin::none) {
        return write_builtin(
            builtin,
            value,
            timestamp_ms,
            apply,
            line);
    }

    Variable* const variable = find_variable(name, true, line);
    if (variable == nullptr) {
        return false;
    }
    if (apply) {
        variable->value = value;
    }
    return true;
}

bool send_message(
    const float value,
    const bool apply,
    const std::uint16_t line)
{
    if (!value_is_valid(value)) {
        set_error(modules::script::Error::numeric_range, line);
        return false;
    }
    if (!apply) {
        return true;
    }

    const std::int32_t integer_value =
        static_cast<std::int32_t>(value);
    const std::uint32_t encoded =
        static_cast<std::uint32_t>(integer_value);
    const std::uint8_t payload[5U]{
        script_message_code,
        static_cast<std::uint8_t>(encoded),
        static_cast<std::uint8_t>(encoded >> 8U),
        static_cast<std::uint8_t>(encoded >> 16U),
        static_cast<std::uint8_t>(encoded >> 24U)};
    if (!modules::event_messages::enqueue(payload, 5U)) {
        set_error(modules::script::Error::send_failed, line);
        return false;
    }

    ++state.snapshot.sent_messages;
    return true;
}

/**
 * @brief Разбирает одну строку и вычисляет выражения с заданным приоритетом.
 */
class LineParser {
public:
    LineParser(
        const char* begin,
        const char* end,
        const std::uint32_t timestamp_ms,
        const std::uint16_t line)
        : position_{begin},
          end_{end},
          timestamp_ms_{timestamp_ms},
          line_{line}
    {
    }

    bool empty()
    {
        skip_spaces();
        return position_ == end_;
    }

    bool read_identifier(
        char (&name)[modules::script::maximum_name_length + 1U])
    {
        skip_spaces();
        if (position_ == end_ || !is_identifier_start(*position_)) {
            set_error(modules::script::Error::syntax, line_);
            return false;
        }

        std::uint8_t length = 0U;
        while (position_ != end_ && is_identifier_part(*position_)) {
            if (length == modules::script::maximum_name_length) {
                set_error(modules::script::Error::name_too_long, line_);
                return false;
            }
            name[length] = upper_ascii(*position_);
            ++length;
            ++position_;
        }
        name[length] = '\0';
        return true;
    }

    bool consume(const char expected)
    {
        skip_spaces();
        if (position_ == end_ || *position_ != expected) {
            return false;
        }
        ++position_;
        return true;
    }

    bool expect(const char expected)
    {
        if (consume(expected)) {
            return true;
        }
        set_error(modules::script::Error::syntax, line_);
        return false;
    }

    bool expect_keyword(const char* expected)
    {
        char actual[modules::script::maximum_name_length + 1U]{};
        if (!read_identifier(actual)) {
            return false;
        }
        if (!names_equal(actual, expected)) {
            set_error(modules::script::Error::syntax, line_);
            return false;
        }
        return true;
    }

    bool finish()
    {
        skip_spaces();
        if (position_ == end_) {
            return true;
        }
        set_error(modules::script::Error::syntax, line_);
        return false;
    }

    bool expression(const bool evaluate, float& value)
    {
        return comparison(evaluate, value);
    }

    bool assign(
        const char* name,
        const bool apply)
    {
        if (!expect('=')) {
            return false;
        }
        float value = 0.0F;
        if (!expression(apply, value) || !finish()) {
            return false;
        }
        return write_value(
            name,
            value,
            timestamp_ms_,
            apply,
            line_);
    }

    bool send(const bool apply)
    {
        if (!expect('(')) {
            return false;
        }
        float value = 0.0F;
        if (!expression(apply, value)
            || !expect(')')
            || !finish()) {
            return false;
        }
        return send_message(value, apply, line_);
    }

private:
    void skip_spaces()
    {
        while (position_ != end_
               && (*position_ == ' '
                   || *position_ == '\t'
                   || *position_ == '\r')) {
            ++position_;
        }
    }

    bool comparison(const bool evaluate, float& value)
    {
        if (!addition(evaluate, value)) {
            return false;
        }

        skip_spaces();
        enum class Operation {
            none,
            equal,
            not_equal,
            less,
            less_equal,
            greater,
            greater_equal
        };
        Operation operation = Operation::none;
        if (position_ != end_ && *position_ == '=') {
            ++position_;
            operation = Operation::equal;
        } else if (position_ != end_ && *position_ == '<') {
            ++position_;
            if (position_ != end_ && *position_ == '=') {
                ++position_;
                operation = Operation::less_equal;
            } else if (position_ != end_ && *position_ == '>') {
                ++position_;
                operation = Operation::not_equal;
            } else {
                operation = Operation::less;
            }
        } else if (position_ != end_ && *position_ == '>') {
            ++position_;
            if (position_ != end_ && *position_ == '=') {
                ++position_;
                operation = Operation::greater_equal;
            } else {
                operation = Operation::greater;
            }
        }

        if (operation == Operation::none) {
            return true;
        }

        float right = 0.0F;
        if (!addition(evaluate, right)) {
            return false;
        }
        if (!evaluate) {
            value = 0.0F;
            return true;
        }

        bool result = false;
        switch (operation) {
        case Operation::equal:
            result = value == right;
            break;
        case Operation::not_equal:
            result = value != right;
            break;
        case Operation::less:
            result = value < right;
            break;
        case Operation::less_equal:
            result = value <= right;
            break;
        case Operation::greater:
            result = value > right;
            break;
        case Operation::greater_equal:
            result = value >= right;
            break;
        case Operation::none:
            break;
        }
        value = result ? 1.0F : 0.0F;
        return true;
    }

    bool addition(const bool evaluate, float& value)
    {
        if (!multiplication(evaluate, value)) {
            return false;
        }

        for (;;) {
            skip_spaces();
            if (position_ == end_
                || (*position_ != '+' && *position_ != '-')) {
                return true;
            }
            const char operation = *position_;
            ++position_;

            float right = 0.0F;
            if (!multiplication(evaluate, right)) {
                return false;
            }
            if (evaluate) {
                value = operation == '+' ? value + right : value - right;
                if (!value_is_valid(value)) {
                    set_error(modules::script::Error::numeric_range, line_);
                    return false;
                }
            }
        }
    }

    bool multiplication(const bool evaluate, float& value)
    {
        if (!unary(evaluate, value)) {
            return false;
        }

        for (;;) {
            skip_spaces();
            if (position_ == end_
                || (*position_ != '*' && *position_ != '/')) {
                return true;
            }
            const char operation = *position_;
            ++position_;

            float right = 0.0F;
            if (!unary(evaluate, right)) {
                return false;
            }
            if (!evaluate) {
                continue;
            }
            if (operation == '/' && right == 0.0F) {
                set_error(modules::script::Error::division_by_zero, line_);
                return false;
            }
            value = operation == '*' ? value * right : value / right;
            if (!value_is_valid(value)) {
                set_error(modules::script::Error::numeric_range, line_);
                return false;
            }
        }
    }

    bool unary(const bool evaluate, float& value)
    {
        skip_spaces();
        bool negative = false;
        std::uint8_t operations = 0U;
        while (position_ != end_
               && (*position_ == '+' || *position_ == '-')) {
            if (operations
                == modules::script::maximum_expression_depth) {
                set_error(modules::script::Error::nesting_limit, line_);
                return false;
            }
            if (*position_ == '-') {
                negative = !negative;
            }
            ++position_;
            ++operations;
            skip_spaces();
        }
        if (!primary(evaluate, value)) {
            return false;
        }
        if (evaluate && negative) {
            value = -value;
        }
        return true;
    }

    bool primary(const bool evaluate, float& value)
    {
        skip_spaces();
        if (position_ == end_) {
            set_error(modules::script::Error::syntax, line_);
            return false;
        }

        if (*position_ == '(') {
            if (expression_depth_
                == modules::script::maximum_expression_depth) {
                set_error(modules::script::Error::nesting_limit, line_);
                return false;
            }
            ++position_;
            ++expression_depth_;
            const bool parsed = expression(evaluate, value);
            --expression_depth_;
            return parsed && expect(')');
        }

        if (is_digit(*position_) || *position_ == '.') {
            return number(value);
        }

        char name[modules::script::maximum_name_length + 1U]{};
        if (!read_identifier(name)) {
            return false;
        }
        return read_value(
            name,
            timestamp_ms_,
            evaluate,
            value,
            line_);
    }

    bool number(float& value)
    {
        skip_spaces();
        bool has_digits = false;
        value = 0.0F;
        while (position_ != end_ && is_digit(*position_)) {
            has_digits = true;
            value =
                value * 10.0F + static_cast<float>(*position_ - '0');
            ++position_;
            if (!value_is_valid(value)) {
                set_error(modules::script::Error::numeric_range, line_);
                return false;
            }
        }

        if (position_ != end_ && *position_ == '.') {
            ++position_;
            float place = 0.1F;
            while (position_ != end_ && is_digit(*position_)) {
                has_digits = true;
                value += static_cast<float>(*position_ - '0') * place;
                place *= 0.1F;
                ++position_;
            }
        }

        if (!has_digits || !value_is_valid(value)) {
            set_error(modules::script::Error::syntax, line_);
            return false;
        }
        return true;
    }

    const char* position_;
    const char* end_;
    std::uint32_t timestamp_ms_;
    std::uint16_t line_;
    std::uint8_t expression_depth_{};
};

bool current_active(
    const BranchFrame* branches,
    const std::uint8_t depth)
{
    return depth == 0U || branches[depth - 1U].active;
}

bool run_program(
    const std::uint32_t timestamp_ms,
    const bool validate)
{
    BranchFrame branches[modules::script::maximum_if_depth]{};
    std::uint8_t branch_depth = 0U;
    std::size_t offset = 0U;
    std::uint16_t line_number = 1U;

    while (offset < state.snapshot.program_size) {
        const std::size_t line_begin = offset;
        while (offset < state.snapshot.program_size
               && state.program[offset] != '\n') {
            ++offset;
        }
        const std::size_t line_length = offset - line_begin;
        if (line_length > modules::script::maximum_line_length) {
            set_error(
                modules::script::Error::line_too_long,
                line_number);
            return false;
        }

        LineParser parser{
            state.program + line_begin,
            state.program + offset,
            timestamp_ms,
            line_number};
        if (!parser.empty()) {
            char statement[modules::script::maximum_name_length + 1U]{};
            if (!parser.read_identifier(statement)) {
                return false;
            }

            if (names_equal(statement, "IF")) {
                if (branch_depth == modules::script::maximum_if_depth) {
                    set_error(
                        modules::script::Error::nesting_limit,
                        line_number);
                    return false;
                }
                const bool parent_active =
                    current_active(branches, branch_depth);
                const bool evaluate = !validate && parent_active;
                float condition = 0.0F;
                if (!parser.expression(evaluate, condition)
                    || !parser.expect_keyword("THEN")
                    || !parser.finish()) {
                    return false;
                }
                BranchFrame& branch = branches[branch_depth];
                branch.parent_active = parent_active;
                branch.condition_true =
                    evaluate && condition != 0.0F;
                branch.active =
                    parent_active && branch.condition_true;
                ++branch_depth;
            } else if (names_equal(statement, "ELSE")) {
                if (branch_depth == 0U
                    || branches[branch_depth - 1U].else_seen
                    || !parser.finish()) {
                    set_error(
                        modules::script::Error::block_mismatch,
                        line_number);
                    return false;
                }
                BranchFrame& branch = branches[branch_depth - 1U];
                branch.else_seen = true;
                branch.active =
                    branch.parent_active && !branch.condition_true;
            } else if (names_equal(statement, "END")) {
                if (!parser.expect_keyword("IF")
                    || !parser.finish()
                    || branch_depth == 0U) {
                    set_error(
                        modules::script::Error::block_mismatch,
                        line_number);
                    return false;
                }
                --branch_depth;
            } else {
                const bool apply =
                    !validate && current_active(branches, branch_depth);
                if (names_equal(statement, "SEND")) {
                    if (!parser.send(apply)) {
                        return false;
                    }
                } else if (!parser.assign(statement, apply)) {
                    return false;
                }
            }
        }

        if (offset < state.snapshot.program_size) {
            ++offset;
        }
        ++line_number;
    }

    if (branch_depth != 0U) {
        set_error(
            modules::script::Error::block_mismatch,
            line_number);
        return false;
    }
    return true;
}

SourceVersions current_sources(const std::uint32_t timestamp_ms)
{
    return SourceVersions{
        timestamp_ms,
        modules::vehicle::snapshot().revision,
        modules::sensors::snapshot().revision,
        modules::configuration::snapshot().revision,
        true};
}

bool sources_changed(const SourceVersions& current)
{
    return !state.sources.captured
        || current.timestamp_ms - state.sources.timestamp_ms
            >= modules::script::execution_interval_ms
        || current.vehicle != state.sources.vehicle
        || current.sensors != state.sources.sensors
        || current.configuration != state.sources.configuration;
}

}  // безымянное пространство имён

namespace modules::script {

void reset()
{
    (void)load(default_program, sizeof(default_program) - 1U);
}

bool load(const char* const program, const std::size_t length)
{
    state = {};
    if (length > maximum_program_size) {
        set_error(Error::program_too_large, 0U);
        return false;
    }
    if (program == nullptr && length != 0U) {
        set_error(Error::syntax, 0U);
        return false;
    }

    for (std::size_t index = 0U; index < length; ++index) {
        state.program[index] = program[index];
    }
    state.program[length] = '\0';
    state.snapshot.loaded = true;
    state.snapshot.program_size =
        static_cast<std::uint16_t>(length);

    if (!run_program(0U, true)) {
        state.snapshot.loaded = false;
        return false;
    }

    for (std::uint8_t index = 0U;
         index < state.snapshot.variables;
         ++index) {
        state.variables[index].value = 0.0F;
    }
    state.snapshot.faulted = false;
    state.snapshot.error = Error::none;
    state.snapshot.error_line = 0U;
    ++state.snapshot.revision;
    return true;
}

void tick(const std::uint32_t timestamp_ms)
{
    if (!state.snapshot.loaded || state.snapshot.faulted) {
        return;
    }

    const SourceVersions before = current_sources(timestamp_ms);
    if (!sources_changed(before)) {
        return;
    }

    const bool completed = run_program(timestamp_ms, false);
    ++state.snapshot.executions;
    state.sources = current_sources(timestamp_ms);
    if (completed) {
        ++state.snapshot.revision;
    }
}

Snapshot snapshot()
{
    return state.snapshot;
}

}  // пространство имён modules::script
