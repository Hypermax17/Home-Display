# STM32F746G-DISCO Port (noch nicht umgesetzt)

`core/` und `ui/` sind bereits fuer Cortex-M7 verifiziert (`cmake -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake -DHD_BUILD_SIM=OFF -DHD_BUILD_TESTS=OFF`):
Kern ~3 KB Flash, UI ~134 KB (davon der Grossteil Fonts), LVGL ~234 KB, LVGL-Heap 96 KB RAM.

Was dieser Port liefern muss (alles Plattformcode, nichts davon beruehrt `core/` oder `ui/`):

| Baustein | Umsetzung | Ersatz fuer (Simulator) |
|---|---|---|
| Takt, Cache, MPU | CubeMX / HAL | - |
| Display | LTDC 480x272 RGB565, Framebuffer im SDRAM (FMC, 8 MB), `lv_display_create` + Flush-Callback (Doppelpuffer/Partial) | `lv_sdl_window_create` |
| Touch | FT5336 ueber I2C (BSP `STM32746G-Discovery`), `lv_indev_create` Pointer | `lv_sdl_mouse_create` |
| Tick | `lv_tick_set_cb(HAL_GetTick)` | `now_ms()` |
| Netzwerk | ETH + lwIP (DHCP oder statische IP); `knx_transport_t` auf UDP-Socket (connect auf Gateway:3671, nicht blockierend) | `platform/sim/sim_udp.c` |
| Tasks (FreeRTOS) | `ui_task`: `lv_timer_handler()` ca. alle 5 ms. `data_task`: `data_service_step(now)` ca. alle 2 ms. Mehr nicht -- die Kommunikation laeuft ueber `app_bus` (lock-frei, kein Mutex noetig). LVGL nur aus `ui_task` ansprechen. | pthread in `platform/sim/main.c` |
| Konfiguration | Gateway-IP/Port, spaeter ueber Einstellungsseite + Flash/QSPI | `--gateway` |

Empfehlung: CubeMX-Projekt (oder STM32CubeIDE/CMake) fuer das Board erzeugen, `core/`, `ui/`, `config/` als Bibliotheken
(`hd_core`, `hd_ui`, `hd_config`) einbinden und `main.c` nach dem Muster von `platform/sim/main.c` schreiben.
