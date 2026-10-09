#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "app_bus.h"
#include "data_service.h"
#include "dpt.h"
#include "knx_ip.h"

#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
static int failures;

/* ---------------- DPT ---------------- */
static void test_dpt(void)
{
    knx_telegram_t t = {0};
    bool b; uint8_t p; int16_t x;

    dpt1_encode(&t, true);  CHECK(t.len == 0 && t.data[0] == 1);
    CHECK(dpt1_decode(&t, &b) && b);

    for (int pc = 0; pc <= 100; pc++) {
        dpt5_encode_percent(&t, (uint8_t)pc);
        CHECK(dpt5_decode_percent(&t, &p) && p == pc);
    }
    dpt5_encode_percent(&t, 100); CHECK(t.data[0] == 255);

    int16_t vals[] = { 0, 215, -50, 214, -273, 1000, 1, -1, 400 };
    for (unsigned i = 0; i < sizeof(vals) / sizeof(vals[0]); i++) {
        dpt9_encode_x10(&t, vals[i]);
        CHECK(dpt9_decode_x10(&t, &x));
        CHECK(x == vals[i]);
    }
    /* Referenzwert: 21,5 degC == 0x0C33 */
    dpt9_encode_x10(&t, 215);
    CHECK(t.data[0] == 0x0C && t.data[1] == 0x33);
    /* Referenzwert: -5,0 degC: m=-500, e=0 -> 0x8000 | (-500 & 0x7FF) = 0x860C */
    dpt9_encode_x10(&t, -50);
    CHECK(t.data[0] == 0x86 && t.data[1] == 0x0C);
}

/* ---------------- cEMI ---------------- */
static void test_cemi(void)
{
    uint8_t buf[32];
    knx_telegram_t t = { .ga = KNX_GA(1, 0, 1), .apci = KNX_APCI_WRITE, .len = 0, .data = {1} };
    size_t n = knx_ip_build_cemi(&t, buf, sizeof(buf));
    const uint8_t ref[] = { 0x11, 0x00, 0xBC, 0xE0, 0x00, 0x00, 0x08, 0x01, 0x01, 0x00, 0x81 };
    CHECK(n == sizeof(ref) && memcmp(buf, ref, n) == 0);

    /* als Indication zurueckparsen */
    buf[0] = 0x29;
    knx_telegram_t r;
    CHECK(knx_ip_parse_cemi(buf, n, &r));
    CHECK(r.ga == t.ga && r.apci == KNX_APCI_WRITE && r.len == 0 && r.data[0] == 1);

    knx_telegram_t t2 = { .ga = KNX_GA(2, 3, 4), .apci = KNX_APCI_RESPONSE, .len = 2, .data = {0x0C, 0x33} };
    n = knx_ip_build_cemi(&t2, buf, sizeof(buf));
    CHECK(n == 13 && buf[8] == 3);
    buf[0] = 0x29;
    CHECK(knx_ip_parse_cemi(buf, n, &r));
    CHECK(r.ga == t2.ga && r.apci == KNX_APCI_RESPONSE && r.len == 2 && r.data[1] == 0x33);

    /* abgeschnittene / kaputte Frames duerfen nicht crashen */
    for (size_t l = 0; l < n; l++) {
        (void)knx_ip_parse_cemi(buf, l, &r);
    }
}

/* ---------------- Fake-Transport / Gateway ---------------- */
typedef struct {
    uint8_t out[16][64]; size_t out_len[16]; int out_n;
    uint8_t in[16][64];  size_t in_len[16];  int in_head, in_tail;
} fake_tp_t;

static bool ft_send(void *c, const uint8_t *b, size_t l)
{
    fake_tp_t *f = c;
    memcpy(f->out[f->out_n], b, l); f->out_len[f->out_n] = l; f->out_n++;
    return true;
}
static int ft_recv(void *c, uint8_t *b, size_t cap)
{
    fake_tp_t *f = c;
    (void)cap;
    if (f->in_head == f->in_tail) return 0;
    int i = f->in_tail++;
    memcpy(b, f->in[i], f->in_len[i]);
    return (int)f->in_len[i];
}
static void ft_feed(fake_tp_t *f, const uint8_t *b, size_t l)
{
    memcpy(f->in[f->in_head], b, l); f->in_len[f->in_head] = l; f->in_head++;
}
static uint16_t svc_of(const uint8_t *b) { return (uint16_t)((b[2] << 8) | b[3]); }

static void test_knx_ip_session(void)
{
    fake_tp_t ft; memset(&ft, 0, sizeof(ft));
    knx_transport_t tp = { &ft, ft_send, ft_recv };
    knx_ip_t k; knx_ip_init(&k, &tp);
    knx_backend_t be = knx_ip_backend(&k);

    CHECK(be.link(be.ctx) == LINK_DOWN);
    knx_telegram_t t = { .ga = KNX_GA(1,0,1), .apci = KNX_APCI_WRITE, .len = 0, .data = {1} };
    CHECK(!be.send(be.ctx, &t)); /* nicht verbunden */

    be.step(be.ctx, 100);
    CHECK(ft.out_n == 1 && svc_of(ft.out[0]) == 0x0205 && ft.out_len[0] == 26);
    CHECK(be.link(be.ctx) == LINK_CONNECTING);

    const uint8_t cres[] = { 0x06,0x10,0x02,0x06,0x00,0x14, 0x07,0x00,
        0x08,0x01,10,0,0,1,0x0E,0x57, 0x04,0x04,0x11,0x05 };
    ft_feed(&ft, cres, sizeof(cres));
    be.step(be.ctx, 150);
    CHECK(be.link(be.ctx) == LINK_UP && k.channel == 7);

    /* senden */
    CHECK(be.send(be.ctx, &t));
    be.step(be.ctx, 160);
    CHECK(ft.out_n == 2 && svc_of(ft.out[1]) == 0x0420);
    CHECK(ft.out[1][7] == 7 && ft.out[1][8] == 0 /* seq */ && ft.out[1][10] == 0x11);

    /* Ack; naechstes Telegramm bekommt seq 1 */
    const uint8_t ack[] = { 0x06,0x10,0x04,0x21,0x00,0x0A, 0x04,0x07,0x00,0x00 };
    ft_feed(&ft, ack, sizeof(ack));
    CHECK(be.send(be.ctx, &t));
    be.step(be.ctx, 170);
    CHECK(ft.out_n == 3 && ft.out[2][8] == 1);

    /* Ack bleibt aus -> 1x Wiederholung nach 1 s, danach Disconnect */
    be.step(be.ctx, 1200);
    CHECK(ft.out_n == 4 && svc_of(ft.out[3]) == 0x0420 && ft.out[3][8] == 1);
    be.step(be.ctx, 2300);
    CHECK(be.link(be.ctx) == LINK_DOWN);
    CHECK(svc_of(ft.out[ft.out_n - 1]) == 0x0209);

    /* Reconnect nach Wartezeit */
    int n0 = ft.out_n;
    be.step(be.ctx, 3000);
    CHECK(ft.out_n == n0);
    be.step(be.ctx, 7400);
    CHECK(ft.out_n == n0 + 1 && svc_of(ft.out[n0]) == 0x0205);
    ft_feed(&ft, cres, sizeof(cres));
    be.step(be.ctx, 7500);
    CHECK(be.link(be.ctx) == LINK_UP);

    /* Eingehendes Tunnelling-Request: quittieren + Telegramm liefern; Duplikat nur quittieren */
    const uint8_t ind[] = { 0x06,0x10,0x04,0x20,0x00,0x15, 0x04,0x07,0x00,0x00,
        0x29,0x00,0xBC,0xE0,0x11,0x02,0x08,0x02,0x01,0x00,0x81 };
    int n1 = ft.out_n;
    ft_feed(&ft, ind, sizeof(ind));
    be.step(be.ctx, 7600);
    CHECK(ft.out_n == n1 + 1 && svc_of(ft.out[n1]) == 0x0421 && ft.out[n1][8] == 0);
    knx_telegram_t r;
    CHECK(be.recv(be.ctx, &r) && r.ga == KNX_GA(1,0,2) && r.apci == KNX_APCI_WRITE && r.data[0] == 1);
    ft_feed(&ft, ind, sizeof(ind));
    be.step(be.ctx, 7700);
    CHECK(ft.out_n == n1 + 2);
    CHECK(!be.recv(be.ctx, &r));

    /* Heartbeat nach 60 s */
    int n2 = ft.out_n;
    be.step(be.ctx, 7500 + 60001);
    CHECK(ft.out_n == n2 + 1 && svc_of(ft.out[n2]) == 0x0207);
}

/* ---------------- Datenservice ---------------- */
static const room_cfg_t t_rooms[] = { { "R", ICON_HOME } };
static const device_cfg_t t_devs[] = {
    { "sw",  0, DEV_SWITCH,      KNX_GA(1,0,1), KNX_GA(1,0,2), 0, 0, 0 },
    { "dim", 0, DEV_DIMMER,      KNX_GA(1,1,1), 0, KNX_GA(1,1,2), 0, 40 },
    { "tmp", 0, DEV_TEMPERATURE, 0, 0, 0, KNX_GA(3,0,1), 200 },
    { "loc", 0, DEV_SWITCH,      0, 0, 0, 0, 0 },
};
static const house_cfg_t t_cfg = { t_rooms, 1, t_devs, 4 };

typedef struct { knx_telegram_t sent[16]; int n_sent; knx_telegram_t rx[8]; int rx_h, rx_t; link_state_t link; bool fail; } spy_t;
static void sp_step(void *c, uint32_t n) { (void)c; (void)n; }
static bool sp_send(void *c, const knx_telegram_t *t) { spy_t *s = c; if (s->fail) return false; s->sent[s->n_sent++] = *t; return true; }
static bool sp_recv(void *c, knx_telegram_t *t) { spy_t *s = c; if (s->rx_h == s->rx_t) return false; *t = s->rx[s->rx_t++]; return true; }
static link_state_t sp_link(void *c) { return ((spy_t *)c)->link; }

static int drain(app_evt_t *ev, int cap)
{
    int n = 0;
    while (n < cap && bus_poll_evt(&ev[n])) n++;
    return n;
}

static void test_data_service(void)
{
    bus_reset();
    spy_t spy; memset(&spy, 0, sizeof(spy));
    knx_backend_t be = { &spy, sp_step, sp_send, sp_recv, sp_link };
    data_service_t ds; data_service_init(&ds, &t_cfg, &be);
    app_evt_t ev[32];

    /* Link down -> kein Senden, Fehler */
    spy.link = LINK_DOWN; spy.fail = true;
    bus_send_cmd(&(app_cmd_t){ CMD_SET_SWITCH, 0, 1 });
    data_service_step(&ds, 0);
    int n = drain(ev, 32);
    bool got_fail = false;
    for (int i = 0; i < n; i++) got_fail |= (ev[i].type == EVT_CMD_FAILED && ev[i].dev == 0);
    CHECK(got_fail);
    spy.fail = false;

    /* Link up -> Sync: Reads auf sw_st und val_st (nur Dimmer hat val_st=GA 1/1/2? nein: val=1/1/2) */
    spy.link = LINK_UP;
    data_service_step(&ds, 10);
    CHECK(spy.n_sent == 2); /* sw_st (1/0/2), temp val_st (3/0/1) */
    CHECK(spy.sent[0].apci == KNX_APCI_READ && spy.sent[0].ga == KNX_GA(1,0,2));
    CHECK(spy.sent[1].apci == KNX_APCI_READ && spy.sent[1].ga == KNX_GA(3,0,1));
    n = drain(ev, 32);
    bool up = false;
    for (int i = 0; i < n; i++) up |= (ev[i].type == EVT_LINK && ev[i].link == LINK_UP);
    CHECK(up);

    /* Schalten mit Rueckmeldeadresse: sendet, aendert Zustand NICHT optimistisch */
    spy.n_sent = 0;
    bus_send_cmd(&(app_cmd_t){ CMD_SET_SWITCH, 0, 1 });
    data_service_step(&ds, 20);
    CHECK(spy.n_sent == 1 && spy.sent[0].ga == KNX_GA(1,0,1) && spy.sent[0].apci == KNX_APCI_WRITE && spy.sent[0].data[0] == 1);
    CHECK(!ds.state[0].on);
    CHECK(drain(ev, 32) == 0);

    /* Rueckmeldung vom Bus -> Zustand + Event */
    spy.rx[spy.rx_h++] = (knx_telegram_t){ .ga = KNX_GA(1,0,2), .apci = KNX_APCI_WRITE, .len = 0, .data = {1} };
    data_service_step(&ds, 30);
    n = drain(ev, 32);
    CHECK(n == 1 && ev[0].type == EVT_DEVICE_STATE && ev[0].dev == 0 && ev[0].state.on && ev[0].state.known);
    /* gleicher Wert nochmal -> kein Event */
    spy.rx[spy.rx_h++] = (knx_telegram_t){ .ga = KNX_GA(1,0,2), .apci = KNX_APCI_RESPONSE, .len = 0, .data = {1} };
    data_service_step(&ds, 40);
    CHECK(drain(ev, 32) == 0);

    /* Dimmer: ohne Rueckmeldeadresse optimistisch, DPT5 */
    spy.n_sent = 0;
    bus_send_cmd(&(app_cmd_t){ CMD_SET_LEVEL, 1, 50 });
    data_service_step(&ds, 50);
    CHECK(spy.n_sent == 1 && spy.sent[0].ga == KNX_GA(1,1,2) && spy.sent[0].len == 1 && spy.sent[0].data[0] == 128);
    n = drain(ev, 32);
    CHECK(n == 1 && ev[0].state.level == 50 && ev[0].state.on);

    /* Temperatur */
    knx_telegram_t tt; dpt9_encode_x10(&tt, 231); tt.ga = KNX_GA(3,0,1); tt.apci = KNX_APCI_WRITE;
    spy.rx[spy.rx_h++] = tt;
    data_service_step(&ds, 60);
    n = drain(ev, 32);
    CHECK(n == 1 && ev[0].dev == 2 && ev[0].state.temp_x10 == 231);

    /* Lokales Geraet ohne GA: Zustand lokal, nichts gesendet */
    spy.n_sent = 0;
    bus_send_cmd(&(app_cmd_t){ CMD_SET_SWITCH, 3, 1 });
    data_service_step(&ds, 70);
    CHECK(spy.n_sent == 0);
    n = drain(ev, 32);
    CHECK(n == 1 && ev[0].dev == 3 && ev[0].state.on);

    /* Refresh: alle Geraete + Link */
    bus_send_cmd(&(app_cmd_t){ CMD_REFRESH_ALL, 0, 0 });
    data_service_step(&ds, 80);
    n = drain(ev, 32);
    CHECK(n == 1 + 4);
}

/* ---------------- Bus ---------------- */
static void test_bus(void)
{
    bus_reset();
    app_cmd_t c = { CMD_SET_SWITCH, 1, 1 }, o;
    for (int i = 0; i < 16; i++) CHECK(bus_send_cmd(&c));
    CHECK(!bus_send_cmd(&c)); /* voll */
    for (int i = 0; i < 16; i++) CHECK(bus_poll_cmd(&o) && o.dev == 1);
    CHECK(!bus_poll_cmd(&o));
}

int main(void)
{
    test_dpt();
    test_cemi();
    test_knx_ip_session();
    test_data_service();
    test_bus();
    if (failures) { printf("%d Fehler\n", failures); return 1; }
    printf("alle Tests ok\n");
    return 0;
}
