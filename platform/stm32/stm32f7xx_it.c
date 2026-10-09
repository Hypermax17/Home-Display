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
extern volatile uint32_t g_boot_stage;

/* Wartet 'cycles' Taktzyklen (DWT-Zaehler). Bricht nach 'cycles' Schleifendurchlaeufen ab, falls der Zaehler
 * nicht laeuft (jeder Durchlauf dauert >= 1 Zyklus, bei funktionierendem Zaehler greift das nie). */
static void cyc_wait(uint32_t cycles)
{
    uint32_t t0 = DWT->CYCCNT;
    uint32_t spin = 0;
    while ((uint32_t)(DWT->CYCCNT - t0) < cycles) {
        if (++spin > cycles) {
            break;
        }
    }
}

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

    /* Blink-Meldung mit konstantem Tempo, unabhaengig vom Takt (Zykluszaehler DWT):
     *   3x Fault-Markierung | N x letzte Boot-Stufe | 1/2/3 x Taktquelle (1 = HSI 16 MHz, 2 = HSE, 3 = PLL 216 MHz) */
    uint32_t sws = (RCC->CFGR & RCC_CFGR_SWS) >> RCC_CFGR_SWS_Pos; /* 0 HSI, 1 HSE, 2 PLL */
    uint32_t hz = (sws == 2) ? 216000000u : (sws == 1 ? 25000000u : 16000000u);
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->LAR = 0xC5ACCE55u; /* Cortex-M7: DWT-Register entsperren */
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    uint32_t cyc_ms = hz / 1000u;

    BSP_LED_Init(LED1);
    for (;;) {
        const uint32_t groups[3] = { 3, g_boot_stage, sws + 1 };
        for (int g = 0; g < 3; g++) {
            if (g == 1 && groups[g] == 0) { /* Stufe 0 = Fault vor main(): ein langer Blink statt Stille */
                BSP_LED_On(LED1);
                cyc_wait(1500u * cyc_ms);
                BSP_LED_Off(LED1);
                cyc_wait(300u * cyc_ms);
            }
            for (uint32_t i = 0; i < groups[g]; i++) {
                BSP_LED_On(LED1);
                cyc_wait(300u * cyc_ms);
                BSP_LED_Off(LED1);
                cyc_wait(300u * cyc_ms);
            }
            cyc_wait((g == 2 ? 3000u : 1200u) * cyc_ms);
        }
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
