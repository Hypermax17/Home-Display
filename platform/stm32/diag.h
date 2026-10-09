#ifndef DIAG_H
#define DIAG_H

#include <stdint.h>

/*
 * Boot-Diagnose ohne Debugger.
 *
 * Die Firmware merkt sich in einem Reset-festen RAM-Bereich (.noinit) den zuletzt abgeschlossenen
 * Boot-Schritt ("Stufe"). Haengt sie (unabhaengiger Watchdog, ~8 s) oder stuerzt sie ab (Fault-Handler
 * loest einen Reset aus), meldet LED1 beim naechsten Start (3 Wiederholungen):
 *
 *   N langsame Blinks   = Stufe N war zuletzt abgeschlossen, der Fehler liegt im Schritt danach
 *   dann 1 schneller Blink = Haenger (Watchdog)    2 schnelle Blinks = CPU-Fault
 *
 * Stufen (siehe main.c): 1 main erreicht, 2 MPU+Caches, 3 HAL_Init, 4 Takt 216 MHz, 5 LCD+SDRAM init,
 * 6 Testbild+Layer, 7 Display an, 8 Touch init, 9 LVGL-Treiber, 10 UI aufgebaut, 11 Hauptschleife.
 * Nach einem Power-Cycle oder Reset-Taster wird nichts gemeldet.
 */
#define DIAG_LOOP_STAGE 11

void diag_start(void);          /* als Allererstes in main() aufrufen: meldet Vorfall, startet Watchdog */
void diag_stage(uint32_t s);    /* Boot-Schritt s abgeschlossen */
void diag_kick(void);           /* Watchdog fuettern */
void diag_fault(void);          /* aus Fault-Handlern: Stufe behalten, Reset ausloesen */

#endif
