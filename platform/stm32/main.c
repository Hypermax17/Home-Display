/*
 * STM32F746G-DISCO: gleiche UI- und Kernschicht wie im Simulator, Backend = Mock (kein Netzwerk).
 *
 * Hardware:   LTDC 480x272 RGB565, Framebuffer im externen SDRAM, FT5336-Touch (I2C)
 * Laufzeit:   eine Hauptschleife (Datenservice + LVGL). UI und Daten sind ueber app_bus
 *             entkoppelt und lassen sich spaeter ohne Aenderung auf zwei RTOS-Tasks verteilen.
 * Diagnose (LED1, gruen, PI1): 1x pro Sekunde blinken = Hauptschleife laeuft. Bei Haenger/Fault meldet
 * LED1 nach dem automatischen Neustart die letzte abgeschlossene Boot-Stufe, siehe diag.h.
 * Beim Start zeigt das Display ~0,7 s lang Farbbalken (rot/gruen/blau) als Test des Anzeigepfads.
 */
#include <string.h>

#include "data_service.h"
#include "diag.h"
#include "knx_backend.h"
#include "lvgl.h"
#include "stm32746g_discovery.h"
#include "stm32746g_discovery_lcd.h"
#include "stm32746g_discovery_sdram.h"
#include "stm32746g_discovery_ts.h"
#include "stm32f7xx_hal.h"
#include "ui.h"

#define LCD_W 480
#define LCD_H 272
#define FB_ADDR SDRAM_DEVICE_ADDR /* 0xC0000000, 480*272*2 = 255 KB */

#define DRAW_BUF_LINES 40
static uint16_t draw_buf1[LCD_W * DRAW_BUF_LINES] __attribute__((aligned(32)));
static uint16_t draw_buf2[LCD_W * DRAW_BUF_LINES] __attribute__((aligned(32)));

static data_service_t g_ds;
static knx_mock_t g_mock;
static knx_backend_t g_backend;

/* Nicht behebbarer Fehler: Watchdog laeuft ab -> Neustart und Meldung der Stufe (siehe diag.h). */
static void fatal(int code)
{
    (void)code;
    for (;;) {}
}

/* SDRAM (Framebuffer) als "Normal, Write-Through, cachebar" einblenden wie in den ST-BSP-Beispielen.
 * Ohne MPU-Konfiguration gilt 0xC0000000 als Device-Speicher (unaligned Zugriffe -> Fault). */
static void mpu_config(void)
{
    MPU_Region_InitTypeDef m = { 0 };

    HAL_MPU_Disable();
    m.Enable = MPU_REGION_ENABLE;
    m.BaseAddress = SDRAM_DEVICE_ADDR;
    m.Size = MPU_REGION_SIZE_16MB;
    m.AccessPermission = MPU_REGION_FULL_ACCESS;
    m.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;
    m.IsCacheable = MPU_ACCESS_CACHEABLE;
    m.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
    m.Number = MPU_REGION_NUMBER0;
    m.TypeExtField = MPU_TEX_LEVEL0;
    m.SubRegionDisable = 0x00;
    m.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
    HAL_MPU_ConfigRegion(&m);
    HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}

/* Testbild: drei Farbbalken, zeigt dass SDRAM -> LTDC -> Panel funktioniert */
static void draw_test_pattern(void)
{
    static const uint16_t col[3] = { 0xF800, 0x07E0, 0x001F };
    uint16_t *fb = (uint16_t *)FB_ADDR;
    for (int y = 0; y < LCD_H; y++) {
        for (int x = 0; x < LCD_W; x++) {
            fb[y * LCD_W + x] = col[x * 3 / LCD_W];
        }
    }
    SCB_CleanDCache_by_Addr((uint32_t *)FB_ADDR, LCD_W * LCD_H * 2);
}

static bool g_touch_ok;

/* 216 MHz aus 25-MHz-HSE (PLL 25/25*432/2), Over-Drive, Flash 7 Waitstates -- wie ST-BSP-Beispiele. */
static void system_clock_config(void)
{
    RCC_OscInitTypeDef osc = { 0 };
    RCC_ClkInitTypeDef clk = { 0 };

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState = RCC_HSE_ON;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLM = 25;
    osc.PLL.PLLN = 432;
    osc.PLL.PLLP = RCC_PLLP_DIV2;
    osc.PLL.PLLQ = 9;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
        for (;;) {}
    }
    if (HAL_PWREx_EnableOverDrive() != HAL_OK) {
        for (;;) {}
    }

    clk.ClockType = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV4;
    clk.APB2CLKDivider = RCC_HCLK_DIV2;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_7) != HAL_OK) {
        for (;;) {}
    }
}

/* ---- LVGL-Treiber ---------------------------------------------------------------------- */

static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px)
{
    const uint16_t *src = (const uint16_t *)px;
    int32_t w = lv_area_get_width(area);

    for (int32_t y = area->y1; y <= area->y2; y++) {
        uint16_t *dst = (uint16_t *)FB_ADDR + (size_t)y * LCD_W + area->x1;
        memcpy(dst, src, (size_t)w * 2);
        src += w;
    }
    /* D-Cache -> SDRAM, damit der LTDC die Pixel sieht (ganze Zeilen: 960 Byte = 30 Cache-Lines) */
    SCB_CleanDCache_by_Addr((uint32_t *)((uint16_t *)FB_ADDR + (size_t)area->y1 * LCD_W),
                            (int32_t)(area->y2 - area->y1 + 1) * LCD_W * 2);
    lv_display_flush_ready(disp);
}

static void touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    static int32_t last_x, last_y;
    TS_StateTypeDef ts;
    (void)indev;

    if (g_touch_ok && BSP_TS_GetState(&ts) == TS_OK && ts.touchDetected) {
        last_x = ts.touchX[0];
        last_y = ts.touchY[0];
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
    data->point.x = last_x;
    data->point.y = last_y;
}

int main(void)
{
    diag_start();
    diag_stage(1); /* main erreicht */
    mpu_config();
    SCB_EnableICache();
    SCB_EnableDCache();
    diag_stage(2);
    HAL_Init();
    diag_stage(3);
    system_clock_config();
    diag_stage(4);

    BSP_LED_On(LED1);

    /* Display: BSP_LCD_Init initialisiert den SDRAM selbst. Danach RGB565-Layer auf den Framebuffer. */
    if (BSP_LCD_Init() != LCD_OK) {
        fatal(2);
    }
    diag_stage(5);
    draw_test_pattern();
    BSP_LCD_LayerRgb565Init(0, FB_ADDR);
    diag_stage(6);
    BSP_LCD_SelectLayer(0);
    BSP_LCD_DisplayOn();
    HAL_Delay(700);
    diag_stage(7);

    /* Touch ist nicht kritisch: ohne Touch laeuft die Anzeige trotzdem */
    g_touch_ok = (BSP_TS_Init(LCD_W, LCD_H) == TS_OK);
    diag_stage(8);

    /* Datenschicht (Mock: Schaltbefehle kommen als Echo zurueck) */
    knx_mock_init(&g_mock);
    g_backend = knx_mock_backend(&g_mock);
    data_service_init(&g_ds, house_config(), &g_backend);

    /* UI */
    lv_init();
    lv_tick_set_cb(HAL_GetTick);

    lv_display_t *disp = lv_display_create(LCD_W, LCD_H);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(disp, draw_buf1, draw_buf2, sizeof(draw_buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(disp, flush_cb);

    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, touch_read_cb);
    diag_stage(9);

    static const ui_info_t info = { "Mock (kein Bus)", "Mockup 0.1 (STM32)" };
    ui_init(disp, &info);
    diag_stage(10);

    uint32_t t_led = 0;
    diag_stage(DIAG_LOOP_STAGE);
    for (;;) {
        diag_kick();
        uint32_t now = HAL_GetTick();
        data_service_step(&g_ds, now);
        lv_timer_handler();
        if ((int32_t)(now - t_led) >= 500) {
            t_led = now;
            BSP_LED_Toggle(LED1);
        }
        HAL_Delay(2);
    }
}
