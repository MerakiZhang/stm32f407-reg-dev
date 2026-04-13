cmake_minimum_required(VERSION 3.15)

project(stm32f407-reg-dev LANGUAGES C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CPU_PARAMS
    -mcpu=cortex-m4
    -mthumb
    -mfpu=fpv4-sp-d16
    -mfloat-abi=hard
)

set(LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/linker/STM32F407ZGTX_FLASH.ld")

set(LINKER_PARAMS
    -T ${LINKER_SCRIPT}
    -Wl,--gc-sections
    -Wl,-Map=${PROJECT_NAME}.map
    ${CPU_PARAMS}
    --specs=nosys.specs
    --specs=nano.specs
)

add_executable(${PROJECT_NAME}
    main.c
    core/src/system_stm32f4xx.c
    peripherals/led/led.c
    peripherals/beep/beep.c
    peripherals/delay/delay.c
    peripherals/sys/sys.c
    peripherals/key/key.c
    peripherals/exti/exti.c
    peripherals/timer/timer.c
    peripherals/uart/uart.c
    peripherals/iwdg/iwdg.c
    startup/startup_stm32f407xx.s
)

target_include_directories(${PROJECT_NAME} PRIVATE
    core/inc
    peripherals/led
    peripherals/beep
    peripherals/delay
    peripherals/sys
    peripherals/key
    peripherals/exti
    peripherals/timer
    peripherals/uart
    peripherals/iwdg
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    STM32F407xx
)

target_compile_options(${PROJECT_NAME} PRIVATE
    ${CPU_PARAMS}
    -ffunction-sections
    -fdata-sections
    -Wall
    -Wextra
    -g3
    $<$<CONFIG:Debug>:-O0>
    $<$<CONFIG:Release>:-O2>
)

target_link_options(${PROJECT_NAME} PRIVATE
    ${LINKER_PARAMS}
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary ${PROJECT_NAME} ${PROJECT_NAME}.bin
    COMMAND ${CMAKE_OBJCOPY} -O ihex ${PROJECT_NAME} ${PROJECT_NAME}.hex
    COMMAND ${CMAKE_SIZE} ${PROJECT_NAME}
    COMMENT "Generating ${PROJECT_NAME}.bin and ${PROJECT_NAME}.hex"
)
