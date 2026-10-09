#include "ui.h"
#include "ui_nav.h"

#define POLL_MS 20
#define MAX_EVENTS_PER_POLL 64

static void poll_cb(lv_timer_t *t)
{
    (void)t;
    app_evt_t e;
    for (int n = 0; n < MAX_EVENTS_PER_POLL && bus_poll_evt(&e); n++) {
        ui_state_apply(&e);
        switch (e.type) {
        case EVT_DEVICE_STATE:
            ui_nav_notify_state(e.dev);
            break;
        case EVT_LINK:
            ui_nav_notify_link();
            break;
        case EVT_CMD_FAILED:
            ui_toast("Befehl nicht gesendet - KNX nicht verbunden");
            break;
        }
    }
}

void ui_init(lv_display_t *disp, const ui_info_t *info)
{
    ui_set_info(info ? info->gateway : NULL, info ? info->version : NULL);
    ui_theme_init(disp);

    /* aktuellen Stand von der Datenschicht anfordern, dann Startseite zeigen */
    ui_cmd_refresh();
    ui_nav_set_root(ui_page_home_create());
    lv_timer_create(poll_cb, POLL_MS, NULL);
}
