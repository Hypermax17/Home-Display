#include "knx_backend.h"

#define MOCK_CAP 8

static void mock_step(void *ctx, uint32_t now_ms)
{
    (void)ctx;
    (void)now_ms;
}

static bool mock_send(void *ctx, const knx_telegram_t *t)
{
    knx_mock_t *m = ctx;
    uint8_t next = (uint8_t)((m->head + 1) % MOCK_CAP);
    if (next == m->tail) {
        return false;
    }
    m->q[m->head] = *t;
    m->q[m->head].apci = KNX_APCI_WRITE; /* Echo als Schreibzugriff, Reads werden nicht beantwortet */
    if (t->apci != KNX_APCI_WRITE) {
        return true;
    }
    m->head = next;
    return true;
}

static bool mock_recv(void *ctx, knx_telegram_t *t)
{
    knx_mock_t *m = ctx;
    if (m->head == m->tail) {
        return false;
    }
    *t = m->q[m->tail];
    m->tail = (uint8_t)((m->tail + 1) % MOCK_CAP);
    return true;
}

static link_state_t mock_link(void *ctx)
{
    (void)ctx;
    return LINK_UP;
}

void knx_mock_init(knx_mock_t *m)
{
    m->head = m->tail = 0;
}

knx_backend_t knx_mock_backend(knx_mock_t *m)
{
    knx_backend_t b = { m, mock_step, mock_send, mock_recv, mock_link };
    return b;
}
