#include "data_service.h"
#include "dpt.h"
#include <string.h>

static void post_state(data_service_t *ds, uint16_t dev)
{
    app_evt_t e = { .type = EVT_DEVICE_STATE, .dev = dev, .state = ds->state[dev] };
    bus_post_evt(&e);
}

static void post_link(data_service_t *ds)
{
    app_evt_t e = { .type = EVT_LINK, .link = ds->link };
    bus_post_evt(&e);
}

static void post_failed(uint16_t dev)
{
    app_evt_t e = { .type = EVT_CMD_FAILED, .dev = dev };
    bus_post_evt(&e);
}

static bool has_feedback(const device_cfg_t *d)
{
    return d->sw_st != KNX_GA_NONE || (d->type != DEV_SWITCH && d->val_st != KNX_GA_NONE);
}

void data_service_init(data_service_t *ds, const house_cfg_t *cfg, const knx_backend_t *be)
{
    memset(ds, 0, sizeof(*ds));
    ds->cfg = cfg;
    ds->be = be;
    ds->link = LINK_DOWN;
    for (size_t i = 0; i < cfg->device_count && i < MAX_DEVICES; i++) {
        const device_cfg_t *d = &cfg->devices[i];
        device_state_t *s = &ds->state[i];
        if (d->type == DEV_TEMPERATURE) {
            s->temp_x10 = d->initial;
        } else {
            s->level = (uint8_t)d->initial;
            s->on = d->initial > 0;
        }
        /* Geraete ohne Busadresse gelten als "bekannt" (reine Demo-Werte) */
        s->known = (d->sw == KNX_GA_NONE && d->sw_st == KNX_GA_NONE &&
                    d->val == KNX_GA_NONE && d->val_st == KNX_GA_NONE);
    }
}

/* Liste der abzufragenden Rueckmeldeadressen: pro Geraet bis zu 2 Eintraege (Index = dev*2 + k). */
static knx_ga_t read_ga(const device_cfg_t *d, unsigned k)
{
    if (k == 0) {
        return d->sw_st != KNX_GA_NONE ? d->sw_st : KNX_GA_NONE;
    }
    if (d->type == DEV_SWITCH) {
        return KNX_GA_NONE;
    }
    return d->val_st;
}

static void step_sync(data_service_t *ds)
{
    size_t total = ds->cfg->device_count * 2;
    while (ds->sync_pos < total) {
        const device_cfg_t *d = &ds->cfg->devices[ds->sync_pos / 2];
        knx_ga_t ga = read_ga(d, ds->sync_pos % 2);
        if (ga != KNX_GA_NONE) {
            knx_telegram_t t = { .ga = ga, .apci = KNX_APCI_READ, .len = 0 };
            if (!ds->be->send(ds->be->ctx, &t)) {
                return; /* Sendepuffer voll -> naechster Durchlauf */
            }
        }
        ds->sync_pos++;
    }
    ds->syncing = false;
}

static void handle_cmd(data_service_t *ds, const app_cmd_t *c)
{
    if (c->type == CMD_REFRESH_ALL) {
        ds->refresh_pending = true;
        ds->refresh_pos = 0;
        post_link(ds);
        return;
    }
    if (c->dev >= ds->cfg->device_count || c->dev >= MAX_DEVICES) {
        return;
    }
    const device_cfg_t *d = &ds->cfg->devices[c->dev];
    device_state_t *s = &ds->state[c->dev];
    knx_telegram_t t = { .apci = KNX_APCI_WRITE };
    bool changed = false;
    bool sent_ok = true;
    bool has_ga;

    if (c->type == CMD_SET_SWITCH && d->type != DEV_TEMPERATURE) {
        has_ga = d->sw != KNX_GA_NONE;
        bool on = c->value != 0;
        if (has_ga) {
            t.ga = d->sw;
            dpt1_encode(&t, on);
            sent_ok = ds->be->send(ds->be->ctx, &t);
        }
        if (sent_ok && !has_feedback(d)) { /* optimistisch, es kommt keine Rueckmeldung */
            s->on = on;
            if (on && d->type == DEV_DIMMER && s->level == 0) {
                s->level = 100;
            }
            s->known = true;
            changed = true;
        }
    } else if (c->type == CMD_SET_LEVEL && d->type == DEV_DIMMER) {
        uint8_t lvl = c->value > 100 ? 100 : c->value;
        has_ga = d->val != KNX_GA_NONE;
        if (has_ga) {
            t.ga = d->val;
            dpt5_encode_percent(&t, lvl);
            sent_ok = ds->be->send(ds->be->ctx, &t);
        }
        if (sent_ok && !has_feedback(d)) {
            s->level = lvl;
            s->on = lvl > 0;
            s->known = true;
            changed = true;
        }
    } else {
        return;
    }

    if (!sent_ok) {
        post_failed(c->dev);
    } else if (changed) {
        post_state(ds, c->dev);
    }
}

static void apply_telegram(data_service_t *ds, const knx_telegram_t *t)
{
    if (t->apci == KNX_APCI_READ) {
        return; /* wir antworten nicht auf Reads (kein Aktor) */
    }
    for (size_t i = 0; i < ds->cfg->device_count && i < MAX_DEVICES; i++) {
        const device_cfg_t *d = &ds->cfg->devices[i];
        device_state_t n = ds->state[i];
        bool v;
        uint8_t pct;
        int16_t x10;
        bool hit = false;

        if (d->type != DEV_TEMPERATURE && t->ga != KNX_GA_NONE && (t->ga == d->sw || t->ga == d->sw_st)) {
            if (dpt1_decode(t, &v)) {
                n.on = v;
                if (v && d->type == DEV_DIMMER && n.level == 0) {
                    n.level = 100;
                }
                hit = true;
            }
        }
        if (d->type == DEV_DIMMER && t->ga != KNX_GA_NONE && (t->ga == d->val || t->ga == d->val_st)) {
            if (dpt5_decode_percent(t, &pct)) {
                n.level = pct;
                if (d->sw_st == KNX_GA_NONE) {
                    n.on = pct > 0;
                }
                hit = true;
            }
        }
        if (d->type == DEV_TEMPERATURE && t->ga != KNX_GA_NONE && t->ga == d->val_st) {
            if (dpt9_decode_x10(t, &x10)) {
                n.temp_x10 = x10;
                hit = true;
            }
        }
        if (hit) {
            n.known = true;
            if (memcmp(&n, &ds->state[i], sizeof(n)) != 0) {
                ds->state[i] = n;
                post_state(ds, (uint16_t)i);
            }
        }
    }
}

void data_service_step(data_service_t *ds, uint32_t now_ms)
{
    ds->be->step(ds->be->ctx, now_ms);

    link_state_t l = ds->be->link(ds->be->ctx);
    if (l != ds->link || !ds->link_published) {
        bool up_now = (l == LINK_UP && ds->link != LINK_UP);
        ds->link = l;
        ds->link_published = true;
        post_link(ds);
        if (up_now) {
            ds->syncing = true;
            ds->sync_pos = 0;
        }
    }
    if (ds->syncing && ds->link == LINK_UP) {
        step_sync(ds);
    }

    /* Kommandos der UI */
    app_cmd_t c;
    while (bus_poll_cmd(&c)) {
        handle_cmd(ds, &c);
    }

    /* Telegramme vom Bus */
    knx_telegram_t t;
    while (ds->be->recv(ds->be->ctx, &t)) {
        apply_telegram(ds, &t);
    }

    /* Vollstaendige Zustandsveroeffentlichung, in Portionen damit die Event-Queue nicht ueberlaeuft */
    if (ds->refresh_pending) {
        for (int n = 0; n < 16 && ds->refresh_pos < ds->cfg->device_count; n++) {
            post_state(ds, ds->refresh_pos++);
        }
        if (ds->refresh_pos >= ds->cfg->device_count) {
            ds->refresh_pending = false;
        }
    }
}
