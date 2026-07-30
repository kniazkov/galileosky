/**
 * @file runtime.cpp
 * @brief Выполняет минимальную инициализацию среды C++ без стандартной libc.
 *
 * Обработчик сброса вызывает runtime_init перед main. Функция последовательно
 * запускает обработчики из секций preinit_array и init_array, благодаря чему
 * поддерживается корректная инициализация глобальных объектов C++. Здесь же
 * находятся операции с памятью, которые оптимизатор вправе создавать сам.
 */

#include <cstddef>

namespace {

using Initializer = void (*)();

extern "C" Initializer __preinit_array_start[];
extern "C" Initializer __preinit_array_end[];
extern "C" Initializer __init_array_start[];
extern "C" Initializer __init_array_end[];

void run_initializers(Initializer* begin, Initializer* end)
{
    while (begin != end) {
        (*begin)();
        ++begin;
    }
}

}  // безымянное пространство имён

/**
 * @brief Заполняет область памяти одним байтом без зависимости от libc.
 */
extern "C" void* memset(
    void* const destination,
    const int value,
    std::size_t count)
{
    auto* current = static_cast<volatile unsigned char*>(destination);
    while (count != 0U) {
        *current = static_cast<unsigned char>(value);
        ++current;
        --count;
    }
    return destination;
}

/**
 * @brief Копирует непересекающиеся области памяти без зависимости от libc.
 */
extern "C" void* memcpy(
    void* const destination,
    const void* const source,
    std::size_t count)
{
    auto* output = static_cast<volatile unsigned char*>(destination);
    auto* input = static_cast<const volatile unsigned char*>(source);
    while (count != 0U) {
        *output = *input;
        ++output;
        ++input;
        --count;
    }
    return destination;
}

extern "C" void runtime_init()
{
    run_initializers(__preinit_array_start, __preinit_array_end);
    run_initializers(__init_array_start, __init_array_end);
}
