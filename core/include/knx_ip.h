#ifndef KNX_IP_H
#define KNX_IP_H

#include <stddef.h>
#include "knx_backend.h"

/*
 * KNXnet/IP-Tunneling-Client (UDP, Unicast) fuer ein KNX-IP-Interface oder knxd.
 * Plattformunabhaengig: das Netzwerk steckt hinter knx_transport_t (UDP-Socket
 * auf dem PC, lwIP-Socket/netconn auf dem STM32). Gateway-IP/-Port kennt nur
 * der Transport.
 */
typedef struct {
    void *ctx;
    /* Ein Datagramm an das Gateway (Port 3671) senden. */
    bool (*send)(void *ctx, const uint8_t *buf, size_t len);
    /* Nicht blockierend: Laenge des empfangenen Datagramms, 0 = nichts da. */
    int (*recv)(void *ctx, uint8_t *buf, size_t cap);
} knx_transport_t;

#define KNX_IP_TXQ 8
#define KNX_IP_RXQ 8

typedef enum {
    KIP_DISCONNECTED = 0,
    KIP_CONNECTING,
    KIP_CONNECTED,
} knx_ip_state_t;

typedef struct {
    knx_transport_t tp;
    knx_ip_state_t state;
    uint8_t channel;
    uint8_t seq_tx;
    uint8_t seq_rx;

    uint32_t t_state;     /* Zeitpunkt des letzten Zustandswechsels / Connect-Requests */
    uint32_t t_heartbeat; /* naechster Heartbeat */
    uint32_t t_hb_sent;   /* wann CONNECTIONSTATE_REQUEST rausging (0 = keiner offen) */
    bool hb_pending;
    uint32_t t_retry;     /* fruehester Zeitpunkt fuer neuen Verbindungsversuch */

    bool tx_pending;
    uint8_t tx_retries;
    uint32_t t_tx;
    uint8_t tx_frame[48];
    size_t tx_frame_len;

    knx_telegram_t txq[KNX_IP_TXQ];
    uint8_t txq_head, txq_tail;
    knx_telegram_t rxq[KNX_IP_RXQ];
    uint8_t rxq_head, rxq_tail;
} knx_ip_t;

void knx_ip_init(knx_ip_t *k, const knx_transport_t *tp);
knx_backend_t knx_ip_backend(knx_ip_t *k);

/* Rohfunktionen (fuer Tests) */
size_t knx_ip_build_cemi(const knx_telegram_t *t, uint8_t *out, size_t cap);
bool knx_ip_parse_cemi(const uint8_t *buf, size_t len, knx_telegram_t *t);

#endif
