#ifndef SIM_HEADLESS_H
#define SIM_HEADLESS_H

#include <stdbool.h>
#include <stdint.h>
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


/* Von aussen gesteuert (Browser-Variante): Zeigerposition/-zustand setzen, Frame abholen */
void sim_headless_pointer(int x, int y, bool down);
const uint16_t *sim_headless_framebuffer(void);  /* 480x272 RGB565 */
bool sim_headless_take_dirty(void);              /* true, wenn seit dem letzten Aufruf neu gezeichnet wurde */

#endif
