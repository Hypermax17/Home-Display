#ifndef DATA_SERVICE_H
#define DATA_SERVICE_H

#include "app_bus.h"
#include "knx_backend.h"

/*
 * Datenschicht: nimmt Kommandos der UI entgegen, uebersetzt sie in KNX-Telegramme
 * (DPT-Kodierung), wertet Telegramme vom Bus aus, haelt den Geraetezustand und
 * veroeffentlicht Aenderungen als Events. Kennt weder LVGL noch Sockets/HAL.
 */
typedef struct {
    const house_cfg_t *cfg;
    const knx_backend_t *be;
    device_state_t state[MAX_DEVICES];
    link_state_t link;
    bool link_published;
    bool syncing;       /* nach Link-Up: Rueckmeldeadressen per GroupValueRead abfragen */
    uint16_t sync_pos;  /* Fortschritt in "Leseliste" */
    bool refresh_pending;
    uint16_t refresh_pos;
} data_service_t;

void data_service_init(data_service_t *ds, const house_cfg_t *cfg, const knx_backend_t *be);
/* Zyklisch (z.B. alle 1..10 ms) aus genau einem Thread/einer Task aufrufen. */
void data_service_step(data_service_t *ds, uint32_t now_ms);

#endif
