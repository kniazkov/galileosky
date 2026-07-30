/**
 * @file runner.cpp
 * @brief Запускает общий tick по миллисекундному таймеру STM32G030.
 *
 * После сброса STM32G030 работает от HSI16. SysTick формирует прерывание раз в
 * миллисекунду, а основной цикл передаёт монотонную 32-битную метку времени в
 * прикладную логику и ожидает следующее прерывание командой wfi.
 */

#include "application/application.hpp"
#include "platform/runtime.hpp"

#include <cstdint>

namespace {

constexpr std::uintptr_t systick_control_address = 0xE000E010UL;
constexpr std::uintptr_t systick_reload_address = 0xE000E014UL;
constexpr std::uintptr_t systick_current_address = 0xE000E018UL;

constexpr std::uint32_t hsi_frequency_hz = 16'000'000U;
constexpr std::uint32_t systick_frequency_hz = 1'000U;
constexpr std::uint32_t systick_reload =
    (hsi_frequency_hz / systick_frequency_hz) - 1U;
constexpr std::uint32_t systick_enable = 1U << 0U;
constexpr std::uint32_t systick_interrupt = 1U << 1U;
constexpr std::uint32_t systick_processor_clock = 1U << 2U;

volatile std::uint32_t milliseconds{};

volatile std::uint32_t& register_at(const std::uintptr_t address)
{
    return *reinterpret_cast<volatile std::uint32_t*>(address);
}

void configure_systick()
{
    register_at(systick_reload_address) = systick_reload;
    register_at(systick_current_address) = 0U;
    register_at(systick_control_address) =
        systick_enable | systick_interrupt | systick_processor_clock;
}

}  // безымянное пространство имён

/**
 * @brief Увеличивает системное время по прерыванию SysTick.
 */
extern "C" void SysTick_Handler()
{
    ++milliseconds;
}

namespace platform {

int run_application()
{
    configure_systick();

    for (;;) {
        const std::uint32_t timestamp_ms = milliseconds;

        application::TickResult result{};
        do {
            result = application::tick(timestamp_ms);
        } while (result.work_pending);

        __asm volatile ("wfi");
    }
}

}  // пространство имён platform
