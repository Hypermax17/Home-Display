/*
 * PC-Simulator: gleiche Kern- und UI-Schicht wie auf dem Zielgeraet, nur
 * Display/Touch (SDL bzw. Headless) und Netzwerk (POSIX-UDP) sind ausgetauscht.
 * Der Datenservice laeuft in einem eigenen Thread -- wie spaeter als RTOS-Task.
 */
#define _POSIX_C_SOURCE 200809L
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "data_service.h"
#include "knx_ip.h"
#include "lvgl.h"
#include "sim_headless.h"
#include "sim_udp.h"
#include "ui.h"

static uint32_t now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000u + ts.tv_nsec / 1000000u);
}

static void sleep_ms(unsigned ms)
{
    struct timespec ts = { ms / 1000, (long)(ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}

static atomic_bool g_run = true;
static data_service_t g_ds;

static void *data_thread(void *arg)
{
    (void)arg;
    while (atomic_load(&g_run)) {
        data_service_step(&g_ds, now_ms());
        sleep_ms(2);
    }
    return NULL;
}

static void usage(const char *a0)
{
    printf("Aufruf: %s [--gateway host[:port]] [--headless --script datei [--shots verzeichnis]]\n"
           "  ohne --gateway: Mock-Backend (kein Netzwerk, alle Geraete lokal)\n"
           "  mit  --gateway: KNXnet/IP-Tunneling zum Gateway (Standardport 3671)\n", a0);
}

int main(int argc, char **argv)
{
    const char *gateway = NULL, *script = NULL, *shots = ".";
    bool headless = false;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--gateway") && i + 1 < argc) gateway = argv[++i];
        else if (!strcmp(argv[i], "--headless")) headless = true;
        else if (!strcmp(argv[i], "--script") && i + 1 < argc) script = argv[++i];
        else if (!strcmp(argv[i], "--shots") && i + 1 < argc) shots = argv[++i];
        else { usage(argv[0]); return 1; }
    }

    /* ---- Datenschicht ---- */
    static knx_mock_t mock;
    static knx_ip_t kip;
    static sim_udp_t udp;
    static knx_backend_t backend;
    static char gw_text[96] = "Mock (kein Bus)";

    if (gateway) {
        char host[64];
        unsigned port = 3671;
        snprintf(host, sizeof(host), "%s", gateway);
        char *colon = strchr(host, ':');
        if (colon) { *colon = 0; port = (unsigned)atoi(colon + 1); }
        if (!sim_udp_open(&udp, host, (uint16_t)port)) {
            return 1;
        }
        knx_transport_t tp = sim_udp_transport(&udp);
        knx_ip_init(&kip, &tp);
        backend = knx_ip_backend(&kip);
        snprintf(gw_text, sizeof(gw_text), "%s:%u", host, port);
    } else {
        knx_mock_init(&mock);
        backend = knx_mock_backend(&mock);
    }
    data_service_init(&g_ds, house_config(), &backend);

    /* ---- UI ---- */
    lv_init();
    lv_tick_set_cb(now_ms);
    lv_display_t *disp;
    if (headless) {
        disp = sim_headless_create(shots, script);
        if (!disp) return 1;
    } else {
        disp = lv_sdl_window_create(480, 272);
        lv_sdl_window_set_title(disp, "Home Display (Simulator)");
        lv_sdl_mouse_create();
    }
    ui_info_t info = { gw_text, "Mockup 0.1" };
    ui_init(disp, &info);

    pthread_t th;
    pthread_create(&th, NULL, data_thread, NULL);

    for (;;) {
        uint32_t wait = lv_timer_handler();
        if (headless && sim_headless_step()) {
            break;
        }
        sleep_ms(wait > 5 ? 5 : wait + 1);
    }

    atomic_store(&g_run, false);
    pthread_join(th, NULL);
    return 0;
}
