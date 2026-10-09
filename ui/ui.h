#ifndef UI_H
#define UI_H

#include "lvgl.h"

/*
 * UI-Schicht. Haengt ausschliesslich von LVGL, model.h und app_bus.h ab --
 * keine KNX-/Netzwerk-/HAL-Includes. Alle LVGL-Aufrufe im selben Thread/derselben
 * Task, die auch lv_timer_handler() ausfuehrt.
 */
typedef struct {
    const char *gateway;  /* Anzeige in den Einstellungen, z.B. "192.168.1.10:3671" oder "Mock (kein Bus)" */
    const char *version;
} ui_info_t;

/* Nach Anlegen des Displays + Eingabegeraets aufrufen; baut Startseite auf. */
void ui_init(lv_display_t *disp, const ui_info_t *info);

#endif
