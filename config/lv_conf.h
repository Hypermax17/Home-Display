/* LVGL-Konfiguration (v9.2). Nicht aufgefuehrte Optionen: Defaults aus lv_conf_internal.h */
#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_DEPTH 16 /* RGB565, passt zum LTDC-Framebuffer des STM32F746G-DISCO */

#ifdef HD_SIM
/* PC-Simulator: Standard-Allocator, SDL-Fenster, Logging */
#define LV_USE_STDLIB_MALLOC LV_STDLIB_CLIB
#define LV_USE_STDLIB_STRING LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF LV_STDLIB_CLIB
#ifndef HD_WEB
#define LV_USE_SDL 1
#endif
#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 1
#else
/* Ziel: eigener Heap im internen RAM (320 KB total; Framebuffer liegt im externen SDRAM) */
#define LV_USE_STDLIB_MALLOC LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_STRING LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_SPRINTF LV_STDLIB_BUILTIN
#define LV_MEM_SIZE (96 * 1024)
#endif

#define LV_USE_OS LV_OS_NONE /* LVGL laeuft nur im UI-Thread/-Task; Kommunikation ueber app_bus */

#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

#define LV_USE_DEMO_WIDGETS 0

#endif
