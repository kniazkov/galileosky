# Тестовый проект для Galileosky

[![Проверка сборки](https://github.com/kniazkov/galileosky/actions/workflows/ci.yml/badge.svg?branch=master)](https://github.com/kniazkov/galileosky/actions/workflows/ci.yml)

Проект собирается как bare-metal прошивка для STM32G030K6T6 и как нативное
desktop-приложение для Linux или Windows. Прикладная логика с общей функцией
`tick` одинакова для всех платформ; платформенные реализации выбираются
конфигурацией CMake. Устройство проекта и протокол стенда описаны в
[ARCHITECTURE.md](ARCHITECTURE.md).

## Целевой микроконтроллер

Используется STM32G030K6T6:

- Arm Cortex-M0+;
- тактовая частота до 64 МГц;
- 32 КБ Flash;
- 8 КБ SRAM.

SRAM разделена linker script на 6 КБ для статических данных и 2 КБ для стека.
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
- GCC с поддержкой C++17;
- Python 3.10 или новее для запуска сценариев.

Debian/Ubuntu:

```bash
sudo apt update
sudo apt install -y cmake ninja-build g++ python3
```

Сборка:

```bash
./scripts/build_linux.sh
```

Сборка и запуск JSONL-стенда:

```bash
./scripts/build_and_run_linux.sh
```

Проверка всех сценариев:

```bash
ctest --test-dir build/linux-desktop --output-on-failure
```

## Desktop-сборка на Windows

Требуются:

- MSYS2 UCRT64;
- CMake 3.21 или новее;
- Ninja;
- MinGW-w64 GCC с поддержкой C++17;
- Python 3.10 или новее для запуска сценариев.

Установка в терминале MSYS2 UCRT64:

```bash
pacman -Syu
pacman -S --needed mingw-w64-ucrt-x86_64-gcc \
  mingw-w64-ucrt-x86_64-cmake \
  mingw-w64-ucrt-x86_64-ninja
```

Каталог `C:\msys64\ucrt64\bin` должен находиться в `PATH`.

Сборка из Command Prompt или PowerShell:

```cmd
scripts\build_windows.cmd
```

Сборка и запуск JSONL-стенда:

```cmd
scripts\build_and_run_windows.cmd
```

Проверка всех сценариев:

```cmd
ctest --test-dir build\windows-desktop --output-on-failure
```

## Сквозная демонстрация

Скрипт `tools/demo_end_to_end.py` запускает собранное desktop-приложение и
проверяет цепочку `CAN → OBD-II → автомобиль → BASIC → сервер`. В консоли
строки с `→` показывают команды, отправленные приложению, а строки с `←` —
полученные ответы. Сценарий сначала передаёт скорость 120 км/ч, затем 145 км/ч
и проверяет серверное сообщение о превышении порога.

Linux:

```bash
./scripts/build_linux.sh
python3 tools/demo_end_to_end.py
```

Windows:

```cmd
scripts\build_windows.cmd
python tools\demo_end_to_end.py
```
