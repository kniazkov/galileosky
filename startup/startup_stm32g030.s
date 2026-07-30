.syntax unified
.cpu cortex-m0plus
.thumb

.extern main
.extern runtime_init
.extern SysTick_Handler

.global Reset_Handler
.global Default_Handler
.global vector_table

.section .isr_vector, "a", %progbits
.type vector_table, %object

vector_table:
    .word _stack_top
    .word Reset_Handler
    .word Default_Handler      /* NMI */
    .word Default_Handler      /* HardFault */
    .word 0                    /* Зарезервировано */
    .word 0                    /* Зарезервировано */
    .word 0                    /* Зарезервировано */
    .word 0                    /* Зарезервировано */
    .word 0                    /* Зарезервировано */
    .word 0                    /* Зарезервировано */
    .word 0                    /* Зарезервировано */
    .word Default_Handler      /* SVCall */
    .word 0                    /* Зарезервировано */
    .word 0                    /* Зарезервировано */
    .word Default_Handler      /* PendSV */
    .word SysTick_Handler      /* Системный таймер */

    /* Внешние прерывания STM32G030. */
    .rept 32
    .word Default_Handler
    .endr

.size vector_table, . - vector_table

.section .text.Reset_Handler, "ax", %progbits
.type Reset_Handler, %function
.thumb_func

Reset_Handler:
    /* Копирование инициализированных данных из Flash в SRAM. */
    ldr r0, =_data_load
    ldr r1, =_data_start
    ldr r2, =_data_end

1:
    cmp r1, r2
    bcs 2f
    ldr r3, [r0]
    adds r0, r0, #4
    str r3, [r1]
    adds r1, r1, #4
    b 1b

    /* Очистка секции неинициализированных данных. */
2:
    movs r0, #0
    ldr r1, =_bss_start
    ldr r2, =_bss_end

3:
    cmp r1, r2
    bcs 4f
    str r0, [r1]
    adds r1, r1, #4
    b 3b

    /* Запуск инициализаторов C++ и вход в приложение. */
4:
    bl runtime_init
    bl main

    /* Защита на случай возврата из main. */
5:
    b 5b

.size Reset_Handler, . - Reset_Handler

.section .text.Default_Handler, "ax", %progbits
.type Default_Handler, %function
.thumb_func

Default_Handler:
    b Default_Handler

.size Default_Handler, . - Default_Handler

.section .note.GNU-stack, "", %progbits
