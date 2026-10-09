#include "app_bus.h"
#include <stdatomic.h>

#define CMD_CAP 16  /* Zweierpotenz */
#define EVT_CAP 128 /* Zweierpotenz, > MAX_DEVICES + Reserve */

#define DEFINE_SPSC(NAME, TYPE, CAP)                                                  \
    static TYPE NAME##_buf[CAP];                                                      \
    static atomic_uint NAME##_head; /* naechster Schreibindex (Producer) */           \
    static atomic_uint NAME##_tail; /* naechster Leseindex (Consumer)    */           \
    static bool NAME##_push(const TYPE *v)                                            \
    {                                                                                 \
        unsigned h = atomic_load_explicit(&NAME##_head, memory_order_relaxed);        \
        unsigned t = atomic_load_explicit(&NAME##_tail, memory_order_acquire);        \
        if (h - t >= (CAP)) {                                                         \
            return false;                                                             \
        }                                                                             \
        NAME##_buf[h & ((CAP)-1)] = *v;                                               \
        atomic_store_explicit(&NAME##_head, h + 1, memory_order_release);             \
        return true;                                                                  \
    }                                                                                 \
    static bool NAME##_pop(TYPE *v)                                                   \
    {                                                                                 \
        unsigned t = atomic_load_explicit(&NAME##_tail, memory_order_relaxed);        \
        unsigned h = atomic_load_explicit(&NAME##_head, memory_order_acquire);        \
        if (h == t) {                                                                 \
            return false;                                                             \
        }                                                                             \
        *v = NAME##_buf[t & ((CAP)-1)];                                               \
        atomic_store_explicit(&NAME##_tail, t + 1, memory_order_release);             \
        return true;                                                                  \
    }

DEFINE_SPSC(cmdq, app_cmd_t, CMD_CAP)
DEFINE_SPSC(evtq, app_evt_t, EVT_CAP)

bool bus_send_cmd(const app_cmd_t *c) { return cmdq_push(c); }
bool bus_poll_cmd(app_cmd_t *c) { return cmdq_pop(c); }
bool bus_post_evt(const app_evt_t *e) { return evtq_push(e); }
bool bus_poll_evt(app_evt_t *e) { return evtq_pop(e); }

void bus_reset(void)
{
    atomic_store(&cmdq_head, 0);
    atomic_store(&cmdq_tail, 0);
    atomic_store(&evtq_head, 0);
    atomic_store(&evtq_tail, 0);
}
