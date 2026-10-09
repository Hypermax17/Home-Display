# STM32F746G-DISCO Firmware

Stand: **Mock-Backend** (kein Netzwerk). Dieselbe `core/`- und `ui/`-Schicht wie im Simulator; hier liegt nur die Plattform:

| Datei | Inhalt |
|---|---|
| `main.c` | Takt (216 MHz), Caches, SDRAM, LTDC (RGB565-Layer), FT5336-Touch, LVGL-Treiber, Hauptschleife |
| `stm32f7xx_hal_conf.h`, `stm32f7xx_it.c` | HAL-Konfiguration, SysTick |
| `STM32F746NGHx_FLASH.ld` | Linker-Skript (1 MB Flash, 320 KB RAM) |
| `CMakeLists.txt` | lädt HAL/CMSIS/BSP automatisch aus dem offiziellen STM32CubeF7 (v1.17.2) und baut `hd_firmware.{elf,bin,hex}` |

Laufzeit: eine Hauptschleife ruft `data_service_step()` und `lv_timer_handler()`. Daten und UI sprechen nur über `app_bus`,
daher lässt sich das später ohne Änderung an `core/`/`ui/` auf zwei FreeRTOS-Tasks verteilen.
Display: LVGL rendert partiell (2 × 480×40 Puffer im internen RAM) und kopiert in den Framebuffer im SDRAM (0xC0000000).

Diagnose über LED1 und Farbbalken-Testbild beim Start: siehe Kommentar am Kopf von `main.c` und die Haupt-README.
SDRAM wird per MPU als Normal/Write-Through eingeblendet; `BSP_LCD_Init()` initialisiert den SDRAM selbst.

Nächster Schritt für echtes KNX: ETH + lwIP, `knx_transport_t` auf einen UDP-Socket, FreeRTOS-Tasks (`ui_task`, `data_task`),
Gateway-Adresse konfigurierbar.
