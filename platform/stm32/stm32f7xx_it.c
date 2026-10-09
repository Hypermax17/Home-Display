#include "stm32746g_discovery.h"
#include "stm32f7xx_hal.h"

/* 1-ms-Takt fuer HAL_GetTick() / LVGL. */
void SysTick_Handler(void)
{
    HAL_IncTick();
}

/* Fault-Diagnose ohne Debugger: LED1 blinkt 3x sehr schnell, Pause, wiederholt. */
static void fault_blink(void)
{
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

void HardFault_Handler(void)   { fault_blink(); }
void MemManage_Handler(void)   { fault_blink(); }
void BusFault_Handler(void)    { fault_blink(); }
void UsageFault_Handler(void)  { fault_blink(); }
