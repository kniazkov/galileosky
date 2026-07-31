cmake_minimum_required(VERSION 3.21)

project(galileosky_test_project LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(GALILEOSKY_PLATFORM "desktop" CACHE STRING
    "Платформа сборки: desktop или stm32g030")
set_property(CACHE GALILEOSKY_PLATFORM PROPERTY STRINGS desktop stm32g030)

add_executable(${PROJECT_NAME}
    src/application.cpp
    src/can.cpp
    src/main.cpp
    src/obd2.cpp
    src/sensor_events.cpp
    src/server_transmission.cpp
    src/server_transport.cpp
    src/sensors.cpp
    src/vehicle.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(GALILEOSKY_PLATFORM STREQUAL "desktop")
    target_sources(${PROJECT_NAME} PRIVATE
        platform/desktop/can_driver.cpp
        platform/desktop/runner.cpp
        platform/desktop/server_transport.cpp
        platform/desktop/sensors.cpp
    )

    include(CTest)
    if(BUILD_TESTING)
        find_package(Python3 REQUIRED COMPONENTS Interpreter)
        file(GLOB GALILEOSKY_SCENARIOS CONFIGURE_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/scenarios/*.json"
        )
        if(NOT GALILEOSKY_SCENARIOS)
            message(FATAL_ERROR "Не найдены JSON-сценарии")
        endif()

        add_test(
            NAME desktop_scenarios
            COMMAND
                ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tools/run_scenarios.py
                --executable $<TARGET_FILE:${PROJECT_NAME}>
                ${GALILEOSKY_SCENARIOS}
        )
    endif()

    if(MSVC)
        target_compile_options(${PROJECT_NAME} PRIVATE /W4 /WX)
    else()
        target_compile_options(${PROJECT_NAME} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Werror
        )
    endif()
elseif(GALILEOSKY_PLATFORM STREQUAL "stm32g030")
    if(NOT CMAKE_CROSSCOMPILING)
        message(FATAL_ERROR
            "Для stm32g030 требуется cmake/arm-none-eabi-gcc.cmake")
    endif()

    enable_language(ASM)

    target_sources(${PROJECT_NAME} PRIVATE
        platform/stm32g030/can_driver.cpp
        platform/stm32g030/runner.cpp
        platform/stm32g030/runtime.cpp
        platform/stm32g030/server_transport.cpp
        platform/stm32g030/sensors.cpp
        startup/startup_stm32g030.s
    )

    target_compile_options(${PROJECT_NAME} PRIVATE
        -mcpu=cortex-m0plus
        -mthumb
        $<$<COMPILE_LANGUAGE:CXX>:-Os>
        $<$<COMPILE_LANGUAGE:CXX>:-ffreestanding>
        $<$<COMPILE_LANGUAGE:CXX>:-ffunction-sections>
        $<$<COMPILE_LANGUAGE:CXX>:-fdata-sections>
        $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
        $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
        $<$<COMPILE_LANGUAGE:CXX>:-fno-threadsafe-statics>
        $<$<COMPILE_LANGUAGE:CXX>:-fno-use-cxa-atexit>
        $<$<COMPILE_LANGUAGE:CXX>:-fno-unwind-tables>
        $<$<COMPILE_LANGUAGE:CXX>:-fno-asynchronous-unwind-tables>
        # Не используем Thumb-1 helper таблиц переходов из libgcc: в некоторых
        # toolchain он не содержит отметку о неисполняемом стеке.
        $<$<COMPILE_LANGUAGE:CXX>:-fno-jump-tables>
        $<$<COMPILE_LANGUAGE:CXX>:-fstack-usage>
        $<$<COMPILE_LANGUAGE:CXX>:-Wa,--noexecstack>
        $<$<COMPILE_LANGUAGE:CXX>:-Wall>
        $<$<COMPILE_LANGUAGE:CXX>:-Wextra>
        $<$<COMPILE_LANGUAGE:CXX>:-Wpedantic>
        $<$<COMPILE_LANGUAGE:CXX>:-Werror>
    )

    target_link_options(${PROJECT_NAME} PRIVATE
        -mcpu=cortex-m0plus
        -mthumb
        -nostdlib
        -Wl,--gc-sections
        -Wl,--fatal-warnings
        -Wl,--print-memory-usage
        -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.map
        -T${CMAKE_CURRENT_SOURCE_DIR}/linker/stm32g030k6.ld
    )

    target_link_libraries(${PROJECT_NAME} PRIVATE gcc)

    set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".elf")

    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O binary
                $<TARGET_FILE:${PROJECT_NAME}>
                ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.bin
        COMMAND ${CMAKE_OBJCOPY} -O ihex
                $<TARGET_FILE:${PROJECT_NAME}>
                ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.hex
        COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${PROJECT_NAME}>
        COMMENT "Формирование BIN/HEX и вывод размера прошивки"
    )
else()
    message(FATAL_ERROR
        "Неизвестная платформа GALILEOSKY_PLATFORM=${GALILEOSKY_PLATFORM}")
endif()
