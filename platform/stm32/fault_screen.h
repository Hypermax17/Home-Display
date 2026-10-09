#ifndef FAULT_SCREEN_H
#define FAULT_SCREEN_H

#include <stdint.h>

/*
 * Zeichnet Text direkt in einen RGB565-Framebuffer (ohne LVGL/HAL), dunkelroter Hintergrund, weisse Schrift.
 * Nur Grossbuchstaben aus "ABCDEFHLMPRS", Ziffern und Leerzeichen. Fuer die Fault-Anzeige gedacht.
 */
void fault_screen_draw(uint16_t *fb, int w, int h, const char *const *lines, int n);

#endif
