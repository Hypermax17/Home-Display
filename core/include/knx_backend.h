#ifndef KNX_BACKEND_H
#define KNX_BACKEND_H

#include "knx_types.h"

/*
 * Abstraktion der KNX-Anbindung. Der Datenservice kennt nur dieses Interface;
 * dahinter steckt KNXnet/IP (knx_ip), ein Loopback-Mock (knx_mock) oder spaeter
 * z.B. ein MQTT-/REST-Gateway.
 *
 * Alle Funktionen werden ausschliesslich aus dem Kontext des Datenservice
 * aufgerufen (ein Thread / eine Task) und duerfen nicht blockieren.
 */
typedef struct knx_backend {
    void *ctx;
    /* Zyklisch aufrufen: Netzwerk bedienen, Verbindung halten. */
    void (*step)(void *ctx, uint32_t now_ms);
    /* Telegramm zum Senden einreihen. false = Link down oder Sendepuffer voll. */
    bool (*send)(void *ctx, const knx_telegram_t *t);
    /* Empfangenes Telegramm abholen. false = nichts da. */
    bool (*recv)(void *ctx, knx_telegram_t *t);
    link_state_t (*link)(void *ctx);
} knx_backend_t;

/* Loopback-Backend ohne Netzwerk: Link immer UP, Schreibzugriffe kommen als Echo zurueck. */
typedef struct {
    knx_telegram_t q[8];
    uint8_t head, tail;
} knx_mock_t;

void knx_mock_init(knx_mock_t *m);
knx_backend_t knx_mock_backend(knx_mock_t *m);

#endif
