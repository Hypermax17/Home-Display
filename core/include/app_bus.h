#ifndef APP_BUS_H
#define APP_BUS_H

#include "model.h"

/*
 * Einzige Schnittstelle zwischen UI und Datenschicht.
 *
 *   UI  --app_cmd_t-->  [cmd queue]  --> Datenservice --> KNX
 *   UI  <--app_evt_t--  [evt queue]  <-- Datenservice <-- KNX
 *
 * Beide Queues sind lock-freie Single-Producer/Single-Consumer-Ringpuffer
 * (C11 atomics). Sie funktionieren damit zwischen zwei Threads/RTOS-Tasks
 * ebenso wie im selben Kontext, ohne Mutex und ohne OS-Abhaengigkeit.
 *
 *   cmd: Producer = UI,            Consumer = Datenservice
 *   evt: Producer = Datenservice,  Consumer = UI
 */

typedef enum {
    CMD_SET_SWITCH,  /* dev, value = 0/1                */
    CMD_SET_LEVEL,   /* dev, value = 0..100 %           */
    CMD_REFRESH_ALL, /* Datenservice veroeffentlicht alle Zustaende + Linkstatus erneut */
} app_cmd_type_t;

typedef struct {
    app_cmd_type_t type;
    uint16_t dev;
    uint8_t value;
} app_cmd_t;

typedef enum {
    EVT_DEVICE_STATE, /* dev, state                                  */
    EVT_LINK,         /* link                                        */
    EVT_CMD_FAILED,   /* dev: Kommando konnte nicht gesendet werden  */
} app_evt_type_t;

typedef struct {
    app_evt_type_t type;
    uint16_t dev;
    device_state_t state;
    link_state_t link;
} app_evt_t;

/* UI-Seite */
bool bus_send_cmd(const app_cmd_t *c);
bool bus_poll_evt(app_evt_t *e);

/* Datenservice-Seite */
bool bus_poll_cmd(app_cmd_t *c);
bool bus_post_evt(const app_evt_t *e);

/* Nur fuer Tests / Neustart: beide Queues leeren (nicht waehrend der Nutzung aufrufen). */
void bus_reset(void);

#endif
