#include "diag.h"
#include "stm32f7xx_hal.h"

/* 1-ms-Takt fuer HAL_GetTick() / LVGL. */
void SysTick_Handler(void)
{
    HAL_IncTick();
}

/* Fault: Stufe merken, Reset ausloesen; die Diagnose (diag.c) meldet das beim naechsten Start per LED1. */
void HardFault_Handler(void)   { diag_fault(); }
void MemManage_Handler(void)   { diag_fault(); }
void BusFault_Handler(void)    { diag_fault(); }
void UsageFault_Handler(void)  { diag_fault(); }
