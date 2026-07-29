# Тестовый проект для Galileosky

Проект собирается как bare-metal прошивка для STM32G030K6T6 и как нативное
desktop-приложение для Linux или Windows. Общая точка входа и интерфейс
приложения одинаковы для всех платформ; платформенные реализации выбираются
конфигурацией CMake.

## Целевой микроконтроллер

Используется STM32G030K6T6:

- Arm Cortex-M0+;
- тактовая частота до 64 МГц;
- 32 КБ Flash;
- 8 КБ SRAM.

SRAM разделена linker script на 7 КБ для статических данных и 1 КБ для стека.
Динамическое выделение памяти в целевой сборке не используется.

## Целевая сборка на Linux

Требуются:

- CMake 3.21 или новее;
- Ninja;
- Arm GNU Toolchain: `arm-none-eabi-g++`, `arm-none-eabi-objcopy`,
  `arm-none-eabi-size`.

Debian/Ubuntu:

```bash
sudo apt update
sudo apt install -y cmake ninja-build gcc-arm-none-eabi binutils-arm-none-eabi
```

Сборка:

```bash
./scripts/build_target.sh
```

Результаты находятся в `build/target`:

- `galileosky_test_project.elf`;
- `galileosky_test_project.bin`;
- `galileosky_test_project.hex`;
- `galileosky_test_project.map`.

## Desktop-сборка на Linux

Требуются:

- CMake 3.21 или новее;
- Ninja;
- GCC с поддержкой C++17.

Debian/Ubuntu:

```bash
sudo apt update
sudo apt install -y cmake ninja-build g++
```

Сборка и запуск:

```bash
./scripts/build_and_run_linux.sh
```

Для остановки приложения используется `Ctrl+C`.

## Desktop-сборка на Windows

Требуются:

- MSYS2 UCRT64;
- CMake 3.21 или новее;
- Ninja;
- MinGW-w64 GCC с поддержкой C++17.

Установка в терминале MSYS2 UCRT64:

```bash
pacman -Syu
pacman -S --needed mingw-w64-ucrt-x86_64-gcc \
  mingw-w64-ucrt-x86_64-cmake \
  mingw-w64-ucrt-x86_64-ninja
```

Каталог `C:\msys64\ucrt64\bin` должен находиться в `PATH`.

Сборка и запуск из Command Prompt или PowerShell:

```cmd
scripts\build_and_run_windows.cmd
```

Для остановки приложения используется `Ctrl+C`.
