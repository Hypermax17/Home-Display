#ifndef SIM_HEADLESS_H
#define SIM_HEADLESS_H

#include <stdbool.h>
#include "lvgl.h"

/*
 * Headless-Anzeige (Framebuffer im RAM) + skriptgesteuerter Touch. Fuer
 * Screenshots und automatisierte Ablaeufe ohne Fenster.
 *
 * Skript (eine Anweisung pro Zeile, '#' = Kommentar):
 *   wait <ms>
 *   tap <x> <y>
 *   drag <x1> <y1> <x2> <y2>
 *   shot <dateiname.ppm>
 *   quit
 */
lv_display_t *sim_headless_create(const char *shot_dir, const char *script_path);
/* Skript-Zustandsmaschine; aus der Hauptschleife aufrufen. true = Skript fertig. */
bool sim_headless_step(void);

#endif
