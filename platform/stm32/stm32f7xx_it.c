#include "stm32f7xx_hal.h"

/* 1-ms-Takt fuer HAL_GetTick() / LVGL. Alle anderen Handler: Default_Handler aus dem Startup-Code. */
void SysTick_Handler(void)
{
    HAL_IncTick();
}
