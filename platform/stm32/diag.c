#include "diag.h"
#include "stm32746g_discovery.h"
#include "stm32f7xx_hal.h"

#define DIAG_MAGIC 0xD1A60002u

/* .noinit (Linker: DTCM @ 0x20000000, nicht gecacht): wird vom Startup-Code weder geloescht noch
 * initialisiert -> ueberlebt Watchdog-/Software-Reset. Im gecachten SRAM ginge der Wert mit dem D-Cache verloren. */
static volatile uint32_t g_magic __attribute__((section(".noinit")));
static volatile uint32_t g_stage __attribute__((section(".noinit")));
static volatile uint32_t g_fault __attribute__((section(".noinit")));

static void wait(uint32_t n)
{
    for (volatile uint32_t i = 0; i < n; i++) {}
}

static void pulse(uint32_t on, uint32_t off)
{
    BSP_LED_On(LED1);
    wait(on);
    BSP_LED_Off(LED1);
    wait(off);
}

void diag_start(void)
{
    uint32_t csr = RCC->CSR;
    RCC->CSR |= RCC_CSR_RMVF;
    uint32_t stage = g_stage, fault = g_fault, magic = g_magic;
    g_magic = DIAG_MAGIC;
    g_stage = 0;
    g_fault = 0;

    BSP_LED_Init(LED1);

    if (magic == DIAG_MAGIC && stage > 0 && (fault || (csr & RCC_CSR_IWDGRSTF))) {
        /* Takt ist hier noch HSI (16 MHz): wait(1500000) ~ 0.3..0.5 s */
        for (int rep = 0; rep < 3; rep++) {
            for (uint32_t i = 0; i < stage; i++) {
                pulse(1500000, 1500000);
            }
            wait(4000000);
            for (int i = 0; i < (fault ? 2 : 1); i++) {
                pulse(400000, 400000);
            }
            wait(9000000);
        }
    }

    /* Unabhaengiger Watchdog: LSI ~32 kHz / 64, Reload 4095 -> ~8 s. Einmal gestartet nicht mehr stoppbar. */
    IWDG->KR = 0x5555;
    IWDG->PR = 4;
    IWDG->RLR = 0x0FFF;
    IWDG->KR = 0xAAAA;
    IWDG->KR = 0xCCCC;
}

void diag_stage(uint32_t s)
{
    g_stage = s;
    IWDG->KR = 0xAAAA;
}

void diag_kick(void)
{
    IWDG->KR = 0xAAAA;
}

void diag_fault(void)
{
    g_fault = 1;
    NVIC_SystemReset();
}
