/**
 * @file runtime.cpp
 * @brief Выполняет минимальную инициализацию среды C++ без стандартной libc.
 *
 * Обработчик сброса вызывает runtime_init перед main. Функция последовательно
 * запускает обработчики из секций preinit_array и init_array, благодаря чему
 * поддерживается корректная инициализация глобальных объектов C++.
 */

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

extern "C" void runtime_init()
{
    run_initializers(__preinit_array_start, __preinit_array_end);
    run_initializers(__init_array_start, __init_array_end);
}
