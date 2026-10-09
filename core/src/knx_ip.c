#include "knx_ip.h"
#include <string.h>

/* Service-Typen */
#define SVC_CONNECT_REQ       0x0205
#define SVC_CONNECT_RES       0x0206
#define SVC_CONNSTATE_REQ     0x0207
#define SVC_CONNSTATE_RES     0x0208
#define SVC_DISCONNECT_REQ    0x0209
#define SVC_DISCONNECT_RES    0x020A
#define SVC_TUNNEL_REQ        0x0420
#define SVC_TUNNEL_ACK        0x0421

#define CEMI_L_DATA_REQ 0x11
#define CEMI_L_DATA_IND 0x29

#define T_CONNECT_TIMEOUT_MS   10000u
#define T_RETRY_DELAY_MS        5000u
#define T_HEARTBEAT_MS         60000u
#define T_HB_TIMEOUT_MS        10000u
#define T_TUNNEL_ACK_MS         1000u

static bool due(uint32_t now, uint32_t t)
{
    return (int32_t)(now - t) >= 0;
}

/* ---- cEMI ------------------------------------------------------------- */

size_t knx_ip_build_cemi(const knx_telegram_t *t, uint8_t *out, size_t cap)
{
    size_t n_data = t->len;
    if (n_data > KNX_MAX_DATA) {
        return 0;
    }
    size_t need = 9 + 2 + n_data;
    if (cap < need) {
        return 0;
    }
    uint8_t apci = (uint8_t)t->apci;
    out[0] = CEMI_L_DATA_REQ;
    out[1] = 0x00;       /* keine Additional Info */
    out[2] = 0xBC;       /* Standard-Frame, normale Prioritaet */
    out[3] = 0xE0;       /* Gruppenadresse, Hop-Count 6 */
    out[4] = 0x00;       /* Quelle 0.0.0: setzt das Interface */
    out[5] = 0x00;
    out[6] = (uint8_t)(t->ga >> 8);
    out[7] = (uint8_t)(t->ga & 0xFF);
    out[9] = (uint8_t)(apci >> 2);               /* TPCI/APCI high */
    if (n_data == 0) {
        out[8] = 0x01;
        out[10] = (uint8_t)(((apci & 3) << 6) | (t->data[0] & 0x3F));
        return 11;
    }
    out[8] = (uint8_t)(n_data + 1);
    out[10] = (uint8_t)((apci & 3) << 6);
    memcpy(&out[11], t->data, n_data);
    return 11 + n_data;
}

bool knx_ip_parse_cemi(const uint8_t *b, size_t len, knx_telegram_t *t)
{
    if (len < 2 || b[0] != CEMI_L_DATA_IND) {
        return false;
    }
    size_t o = 2u + b[1]; /* Additional Info ueberspringen */
    if (len < o + 8 + 1) {
        return false;
    }
    uint8_t ctrl2 = b[o + 1];
    if (!(ctrl2 & 0x80)) {
        return false; /* kein Gruppentelegramm */
    }
    uint16_t dst = (uint16_t)((b[o + 4] << 8) | b[o + 5]);
    uint8_t dlen = b[o + 6];
    if (dlen < 1 || dlen > KNX_MAX_DATA + 1 || len < o + 7 + 1 + dlen) {
        return false;
    }
    const uint8_t *tpdu = &b[o + 7];
    uint8_t apci = (uint8_t)(((tpdu[0] & 0x03) << 2) | (tpdu[1] >> 6));
    if (apci > KNX_APCI_WRITE) {
        return false;
    }
    memset(t, 0, sizeof(*t));
    t->ga = dst;
    t->apci = (knx_apci_t)apci;
    if (dlen == 1) {
        t->len = 0;
        t->data[0] = tpdu[1] & 0x3F;
    } else {
        t->len = (uint8_t)(dlen - 1);
        memcpy(t->data, &tpdu[2], t->len);
    }
    return true;
}

/* ---- Frames ----------------------------------------------------------- */

static size_t put_header(uint8_t *b, uint16_t svc, size_t total)
{
    b[0] = 0x06;
    b[1] = 0x10;
    b[2] = (uint8_t)(svc >> 8);
    b[3] = (uint8_t)svc;
    b[4] = (uint8_t)(total >> 8);
    b[5] = (uint8_t)total;
    return 6;
}

/* HPAI "NAT-Modus": 0.0.0.0:0 -> Gateway antwortet an die Absenderadresse des Pakets */
static size_t put_hpai(uint8_t *b)
{
    b[0] = 0x08;
    b[1] = 0x01; /* UDP */
    memset(&b[2], 0, 6);
    return 8;
}

static void send_connect(knx_ip_t *k)
{
    uint8_t f[26];
    size_t o = put_header(f, SVC_CONNECT_REQ, sizeof(f));
    o += put_hpai(&f[o]);
    o += put_hpai(&f[o]);
    f[o++] = 0x04; /* CRI */
    f[o++] = 0x04; /* TUNNEL_CONNECTION */
    f[o++] = 0x02; /* KNXnet/IP Tunnel Link Layer */
    f[o++] = 0x00;
    k->tp.send(k->tp.ctx, f, o);
}

static void send_chan_req(knx_ip_t *k, uint16_t svc)
{
    uint8_t f[16];
    size_t o = put_header(f, svc, sizeof(f));
    f[o++] = k->channel;
    f[o++] = 0x00;
    o += put_hpai(&f[o]);
    k->tp.send(k->tp.ctx, f, o);
}

static void send_tunnel_ack(knx_ip_t *k, uint8_t seq, uint8_t status)
{
    uint8_t f[10];
    size_t o = put_header(f, SVC_TUNNEL_ACK, sizeof(f));
    f[o++] = 0x04;
    f[o++] = k->channel;
    f[o++] = seq;
    f[o++] = status;
    k->tp.send(k->tp.ctx, f, o);
}

static void send_disconnect_res(knx_ip_t *k, uint8_t channel)
{
    uint8_t f[8];
    size_t o = put_header(f, SVC_DISCONNECT_RES, sizeof(f));
    f[o++] = channel;
    f[o++] = 0x00;
    k->tp.send(k->tp.ctx, f, o);
}

static void to_disconnected(knx_ip_t *k, uint32_t now, bool notify_gateway)
{
    if (notify_gateway && k->state == KIP_CONNECTED) {
        send_chan_req(k, SVC_DISCONNECT_REQ);
    }
    k->state = KIP_DISCONNECTED;
    k->tx_pending = false;
    k->hb_pending = false;
    k->txq_head = k->txq_tail = 0; /* alte Kommandos nicht nach Reconnect nachschieben */
    k->t_retry = now + T_RETRY_DELAY_MS;
}

/* ---- Empfang ---------------------------------------------------------- */

static void rxq_push(knx_ip_t *k, const knx_telegram_t *t)
{
    uint8_t next = (uint8_t)((k->rxq_head + 1) % KNX_IP_RXQ);
    if (next == k->rxq_tail) {
        return; /* voll -> verwerfen */
    }
    k->rxq[k->rxq_head] = *t;
    k->rxq_head = next;
}

static void handle_frame(knx_ip_t *k, const uint8_t *b, size_t len, uint32_t now)
{
    if (len < 6 || b[0] != 0x06 || b[1] != 0x10) {
        return;
    }
    uint16_t svc = (uint16_t)((b[2] << 8) | b[3]);
    size_t total = (size_t)((b[4] << 8) | b[5]);
    if (total > len || total < 6) {
        return;
    }
    const uint8_t *p = b + 6;
    size_t plen = total - 6;

    switch (svc) {
    case SVC_CONNECT_RES:
        if (k->state != KIP_CONNECTING || plen < 2) {
            return;
        }
        if (p[1] == 0x00) { /* E_NO_ERROR */
            k->channel = p[0];
            k->seq_tx = 0;
            k->seq_rx = 0;
            k->state = KIP_CONNECTED;
            k->t_heartbeat = now + T_HEARTBEAT_MS;
            k->hb_pending = false;
            k->tx_pending = false;
        } else {
            to_disconnected(k, now, false);
        }
        break;

    case SVC_CONNSTATE_RES:
        if (k->state == KIP_CONNECTED && plen >= 2 && p[0] == k->channel) {
            if (p[1] == 0x00) {
                k->hb_pending = false;
                k->t_heartbeat = now + T_HEARTBEAT_MS;
            } else {
                to_disconnected(k, now, false);
            }
        }
        break;

    case SVC_DISCONNECT_REQ:
        if (k->state != KIP_DISCONNECTED && plen >= 2 && p[0] == k->channel) {
            send_disconnect_res(k, p[0]);
            to_disconnected(k, now, false);
        }
        break;

    case SVC_DISCONNECT_RES:
        break;

    case SVC_TUNNEL_ACK:
        if (k->state == KIP_CONNECTED && plen >= 4 && p[1] == k->channel && k->tx_pending &&
            p[2] == k->seq_tx) {
            if (p[3] == 0x00) {
                k->seq_tx++;
                k->tx_pending = false;
            } else {
                to_disconnected(k, now, true);
            }
        }
        break;

    case SVC_TUNNEL_REQ:
        if (k->state != KIP_CONNECTED || plen < 4 || p[0] != 0x04 || p[1] != k->channel) {
            return;
        }
        {
            uint8_t seq = p[2];
            if (seq == k->seq_rx) {
                send_tunnel_ack(k, seq, 0x00);
                k->seq_rx++;
                knx_telegram_t t;
                if (knx_ip_parse_cemi(p + 4, plen - 4, &t)) {
                    rxq_push(k, &t);
                }
            } else if (seq == (uint8_t)(k->seq_rx - 1)) {
                send_tunnel_ack(k, seq, 0x00); /* Wiederholung: nur quittieren */
            }
        }
        break;

    default:
        break;
    }
}

/* ---- Backend ---------------------------------------------------------- */

static void kip_step(void *ctx, uint32_t now)
{
    knx_ip_t *k = ctx;
    uint8_t buf[64];
    int n;

    while ((n = k->tp.recv(k->tp.ctx, buf, sizeof(buf))) > 0) {
        handle_frame(k, buf, (size_t)n, now);
    }

    switch (k->state) {
    case KIP_DISCONNECTED:
        if (due(now, k->t_retry)) {
            send_connect(k);
            k->state = KIP_CONNECTING;
            k->t_state = now;
        }
        break;

    case KIP_CONNECTING:
        if (due(now, k->t_state + T_CONNECT_TIMEOUT_MS)) {
            to_disconnected(k, now, false);
        }
        break;

    case KIP_CONNECTED:
        /* Heartbeat */
        if (!k->hb_pending && due(now, k->t_heartbeat)) {
            send_chan_req(k, SVC_CONNSTATE_REQ);
            k->hb_pending = true;
            k->t_hb_sent = now;
        } else if (k->hb_pending && due(now, k->t_hb_sent + T_HB_TIMEOUT_MS)) {
            to_disconnected(k, now, true);
            break;
        }
        /* Senden: immer nur ein Telegramm unquittiert */
        if (k->tx_pending) {
            if (due(now, k->t_tx + T_TUNNEL_ACK_MS)) {
                if (k->tx_retries == 0) {
                    k->tx_retries = 1;
                    k->t_tx = now;
                    k->tp.send(k->tp.ctx, k->tx_frame, k->tx_frame_len);
                } else {
                    to_disconnected(k, now, true);
                }
            }
        } else if (k->txq_tail != k->txq_head) {
            uint8_t cemi[24];
            size_t cl = knx_ip_build_cemi(&k->txq[k->txq_tail], cemi, sizeof(cemi));
            k->txq_tail = (uint8_t)((k->txq_tail + 1) % KNX_IP_TXQ);
            if (cl > 0) {
                size_t o = put_header(k->tx_frame, SVC_TUNNEL_REQ, 6 + 4 + cl);
                k->tx_frame[o++] = 0x04;
                k->tx_frame[o++] = k->channel;
                k->tx_frame[o++] = k->seq_tx;
                k->tx_frame[o++] = 0x00;
                memcpy(&k->tx_frame[o], cemi, cl);
                k->tx_frame_len = o + cl;
                k->tx_pending = true;
                k->tx_retries = 0;
                k->t_tx = now;
                k->tp.send(k->tp.ctx, k->tx_frame, k->tx_frame_len);
            }
        }
        break;
    }
}

static bool kip_send(void *ctx, const knx_telegram_t *t)
{
    knx_ip_t *k = ctx;
    if (k->state != KIP_CONNECTED) {
        return false;
    }
    uint8_t next = (uint8_t)((k->txq_head + 1) % KNX_IP_TXQ);
    if (next == k->txq_tail) {
        return false;
    }
    k->txq[k->txq_head] = *t;
    k->txq_head = next;
    return true;
}

static bool kip_recv(void *ctx, knx_telegram_t *t)
{
    knx_ip_t *k = ctx;
    if (k->rxq_head == k->rxq_tail) {
        return false;
    }
    *t = k->rxq[k->rxq_tail];
    k->rxq_tail = (uint8_t)((k->rxq_tail + 1) % KNX_IP_RXQ);
    return true;
}

static link_state_t kip_link(void *ctx)
{
    const knx_ip_t *k = ctx;
    switch (k->state) {
    case KIP_CONNECTED:  return LINK_UP;
    case KIP_CONNECTING: return LINK_CONNECTING;
    default:             return LINK_DOWN;
    }
}

void knx_ip_init(knx_ip_t *k, const knx_transport_t *tp)
{
    memset(k, 0, sizeof(*k));
    k->tp = *tp;
    k->state = KIP_DISCONNECTED;
    k->t_retry = 0;
}

knx_backend_t knx_ip_backend(knx_ip_t *k)
{
    knx_backend_t b = { k, kip_step, kip_send, kip_recv, kip_link };
    return b;
}
