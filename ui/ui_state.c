#include "ui_state.h"
#include <stdio.h>
#include <stdlib.h>

static device_state_t g_dev[MAX_DEVICES];
static link_state_t g_link = LINK_DOWN;

const device_state_t *ui_state_dev(uint16_t dev)
{
    return &g_dev[dev < MAX_DEVICES ? dev : 0];
}

link_state_t ui_state_link(void)
{
    return g_link;
}

void ui_state_apply(const app_evt_t *e)
{
    if (e->type == EVT_DEVICE_STATE && e->dev < MAX_DEVICES) {
        g_dev[e->dev] = e->state;
    } else if (e->type == EVT_LINK) {
        g_link = e->link;
    }
}

void ui_cmd_set_switch(uint16_t dev, bool on)
{
    app_cmd_t c = { CMD_SET_SWITCH, dev, on ? 1 : 0 };
    bus_send_cmd(&c);
}

void ui_cmd_set_level(uint16_t dev, uint8_t percent)
{
    app_cmd_t c = { CMD_SET_LEVEL, dev, percent };
    bus_send_cmd(&c);
}

void ui_cmd_refresh(void)
{
    app_cmd_t c = { CMD_REFRESH_ALL, 0, 0 };
    bus_send_cmd(&c);
}

int ui_room_lights_on(uint8_t room)
{
    const house_cfg_t *h = house_config();
    int n = 0;
    for (size_t i = 0; i < h->device_count && i < MAX_DEVICES; i++) {
        const device_cfg_t *d = &h->devices[i];
        if (d->room == room && d->type != DEV_TEMPERATURE && g_dev[i].on) {
            n++;
        }
    }
    return n;
}

int ui_room_light_count(uint8_t room)
{
    const house_cfg_t *h = house_config();
    int n = 0;
    for (size_t i = 0; i < h->device_count; i++) {
        if (h->devices[i].room == room && h->devices[i].type != DEV_TEMPERATURE) {
            n++;
        }
    }
    return n;
}

bool ui_room_temperature(uint8_t room, int16_t *temp_x10)
{
    const house_cfg_t *h = house_config();
    for (size_t i = 0; i < h->device_count && i < MAX_DEVICES; i++) {
        if (h->devices[i].room == room && h->devices[i].type == DEV_TEMPERATURE && g_dev[i].known) {
            *temp_x10 = g_dev[i].temp_x10;
            return true;
        }
    }
    return false;
}

void ui_format_temp(char *buf, size_t n, int16_t x10)
{
    int a = abs(x10);
    snprintf(buf, n, "%s%d,%d °C", x10 < 0 ? "-" : "", a / 10, a % 10);
}
