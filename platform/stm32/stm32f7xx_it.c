#include "fault_screen.h"
#include "stm32746g_discovery.h"
#include "stm32f7xx_hal.h"

/* 1-ms-Takt fuer HAL_GetTick() / LVGL. */
void SysTick_Handler(void)
{
    HAL_IncTick();
}

/*
 * CPU-Fault: Register auf dem Display anzeigen (Framebuffer im SDRAM, sofern das Display schon laeuft)
 * und LED1 3x kurz blinken lassen. Eigener Notfall-Stack, falls der normale Stack die Ursache war.
 *
 *   PC/LR   Adresse der fehlerhaften Anweisung / Ruecksprung  (mit arm-none-eabi-addr2line nachschlagbar)
 *   CFSR    Fault-Ursache (Bit 16 = UNDEFINSTR, 17 = INVSTATE, 18 = INVPC, 19 = NOCP, 24 = UNALIGNED, 25 = DIVBYZERO,
 *           Bit 8 = IBUSERR, 9 = PRECISERR, 15 = BFARVALID, Bit 0 = IACCVIOL, 1 = DACCVIOL, 7 = MMARVALID)
 *   HFSR    Bit 30 = FORCED (aus anderem Fault eskaliert)
 *   BFAR    Adresse bei Busfehler        MMFAR  Adresse bei MPU-Verletzung
 *   SP      Stackzeiger zum Zeitpunkt des Faults        HP  Ende von .bss (Stack darf nicht darunter fallen)
 */
extern uint32_t end; /* Linker-Symbol: Ende von .bss */

uint32_t fault_stack[512] __attribute__((aligned(8), used)); /* wird vom Assembler-Trampolin referenziert */

static void hex8(char *out, uint32_t v)
{
    static const char d[] = "0123456789ABCDEF";
    for (int i = 0; i < 8; i++) {
        out[i] = d[(v >> (28 - 4 * i)) & 0xF];
    }
}

void fault_c(uint32_t *frame)
{
    static char l_pc[] = "PC 00000000", l_lr[] = "LR 00000000", l_cf[] = "CFSR 00000000", l_hf[] = "HFSR 00000000";
    static char l_bf[] = "BFAR 00000000", l_mm[] = "MMFAR 00000000", l_sp[] = "SP 00000000", l_hp[] = "HP 00000000";
    static const char *lines[] = { l_pc, l_lr, l_cf, l_hf, l_bf, l_mm, l_sp, l_hp };

    uint32_t fp = (uint32_t)frame;
    int ok = fp >= 0x20000000u && fp <= 0x20050000u - 32u; /* gestackter Rahmen im RAM? */

    hex8(l_pc + 3, ok ? frame[6] : 0);
    hex8(l_lr + 3, ok ? frame[5] : 0);
    hex8(l_cf + 5, SCB->CFSR);
    hex8(l_hf + 5, SCB->HFSR);
    hex8(l_bf + 5, SCB->BFAR);
    hex8(l_mm + 6, SCB->MMFAR);
    hex8(l_sp + 3, fp);
    hex8(l_hp + 3, (uint32_t)&end);

    /* Nur zeichnen, wenn das Display schon laeuft (LTDC getaktet und aktiv): sonst waere SDRAM nicht erreichbar,
     * der Zugriff wuerde einen weiteren Fehler ausloesen. */
    if ((RCC->APB2ENR & RCC_APB2ENR_LTDCEN) && (LTDC->GCR & LTDC_GCR_LTDCEN)) {
        fault_screen_draw((uint16_t *)0xC0000000u, 480, 272, lines, 8);
    }

    BSP_LED_Init(LED1);
    for (;;) {
        for (int i = 0; i < 3; i++) {
            BSP_LED_On(LED1);
            for (volatile uint32_t n = 0; n < 1200000; n++) {}
            BSP_LED_Off(LED1);
            for (volatile uint32_t n = 0; n < 1200000; n++) {}
        }
        for (volatile uint32_t n = 0; n < 20000000; n++) {}
    }
}

/* Rahmenzeiger (MSP oder PSP) ermitteln, auf Notfall-Stack wechseln, fault_c(frame) aufrufen. */
#define FAULT_TRAMPOLINE(name)                                       \
    __attribute__((naked)) void name(void)                           \
    {                                                                \
        __asm volatile("tst lr, #4\n"                                \
                       "ite eq\n"                                    \
                       "mrseq r0, msp\n"                             \
                       "mrsne r0, psp\n"                             \
                       "ldr r1, =fault_stack + 2048\n"               \
                       "mov sp, r1\n"                                \
                       "b fault_c\n");                               \
    }

FAULT_TRAMPOLINE(HardFault_Handler)
FAULT_TRAMPOLINE(MemManage_Handler)
FAULT_TRAMPOLINE(BusFault_Handler)
FAULT_TRAMPOLINE(UsageFault_Handler)
