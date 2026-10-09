#include "ui_nav.h"
#include <stdio.h>

#define CARD_W ((UI_SCREEN_W - 3 * UI_PAD) / 2)
#define CARD_H 88
#define SLIDER_SEND_MS 120

typedef struct {
    uint16_t dev;
    lv_obj_t *card;
    lv_obj_t *sw;      /* Switch / Dimmer */
    lv_obj_t *slider;  /* Dimmer */
    lv_obj_t *value;   /* Status-/Prozent-/Temperaturtext */
    uint32_t last_send;
} card_t;

typedef struct {
    card_t cards[24];
    size_t n;
} room_priv_t;

static void card_refresh(card_t *c)
{
    const device_cfg_t *d = &house_config()->devices[c->dev];
    const device_state_t *s = ui_state_dev(c->dev);
    char buf[24];

    if (d->type == DEV_TEMPERATURE) {
        if (s->known) {
            ui_format_temp(buf, sizeof(buf), s->temp_x10);
            lv_label_set_text(c->value, buf);
        } else {
            lv_label_set_text(c->value, "--");
        }
        return;
    }

    bool on = s->on;
    lv_obj_set_style_border_width(c->card, on ? 2 : 0, 0);
    lv_obj_set_style_border_color(c->card, UI_COL_ACCENT, 0);
    if (on) {
        lv_obj_add_state(c->sw, LV_STATE_CHECKED);
    } else {
        lv_obj_remove_state(c->sw, LV_STATE_CHECKED);
    }
    if (d->type == DEV_DIMMER) {
        snprintf(buf, sizeof(buf), "%d %%", s->level);
        lv_label_set_text(c->value, buf);
        if (!lv_slider_is_dragged(c->slider)) {
            lv_slider_set_value(c->slider, s->level, LV_ANIM_OFF);
        }
    } else {
        lv_label_set_text(c->value, on ? "Ein" : "Aus");
        lv_obj_set_style_text_color(c->value, on ? UI_COL_ACCENT : UI_COL_MUTED, 0);
    }
}

static card_t *card_of(lv_event_t *e)
{
    return lv_event_get_user_data(e);
}

static void switch_card_clicked(lv_event_t *e)
{
    card_t *c = card_of(e);
    ui_cmd_set_switch(c->dev, !ui_state_dev(c->dev)->on);
}

static void dimmer_switch_clicked(lv_event_t *e)
{
    card_t *c = card_of(e);
    ui_cmd_set_switch(c->dev, !ui_state_dev(c->dev)->on);
    /* Widget zeigt bis zur Rueckmeldung den alten Zustand */
    card_refresh(c);
}

static void dimmer_card_clicked(lv_event_t *e)
{
    ui_nav_push(ui_page_device_create(card_of(e)->dev));
}

static void slider_changed(lv_event_t *e)
{
    card_t *c = card_of(e);
    lv_obj_t *s = lv_event_get_target(e);
    int v = lv_slider_get_value(s);
    char buf[16];
    snprintf(buf, sizeof(buf), "%d %%", v);
    lv_label_set_text(c->value, buf);

    uint32_t now = lv_tick_get();
    bool released = lv_event_get_code(e) == LV_EVENT_RELEASED;
    if (released || (now - c->last_send) >= SLIDER_SEND_MS) {
        c->last_send = now;
        ui_cmd_set_level(c->dev, (uint8_t)v);
    }
}

static void room_on_state(ui_page_t *p, uint16_t dev)
{
    room_priv_t *r = p->priv;
    for (size_t i = 0; i < r->n; i++) {
        if (r->cards[i].dev == dev) {
            card_refresh(&r->cards[i]);
        }
    }
}

static void room_on_resume(ui_page_t *p)
{
    room_priv_t *r = p->priv;
    for (size_t i = 0; i < r->n; i++) {
        card_refresh(&r->cards[i]);
    }
}

static lv_obj_t *mk_label(lv_obj_t *parent, const lv_font_t *f, lv_color_t col)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, f, 0);
    lv_obj_set_style_text_color(l, col, 0);
    return l;
}

ui_page_t *ui_page_room_create(uint8_t room)
{
    const house_cfg_t *cfg = house_config();
    ui_page_t *p = ui_page_create(cfg->rooms[room].name, true, false);
    room_priv_t *r = ui_page_alloc_priv(p, sizeof(*r));
    p->on_state = room_on_state;
    p->on_resume = room_on_resume;

    lv_obj_set_flex_flow(p->content, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_add_flag(p->content, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(p->content, LV_DIR_VER);

    for (size_t i = 0; i < cfg->device_count && r->n < 24; i++) {
        const device_cfg_t *d = &cfg->devices[i];
        if (d->room != room) {
            continue;
        }
        card_t *c = &r->cards[r->n++];
        c->dev = (uint16_t)i;
        c->card = ui_card_create(p->content, CARD_W, CARD_H);

        lv_obj_t *name = mk_label(c->card, &hd_font_text_20, UI_COL_TEXT);
        lv_label_set_text(name, d->name);
        ui_label_oneline(name, &hd_font_text_20, CARD_W - 20);
        lv_obj_align(name, LV_ALIGN_TOP_LEFT, 0, 0);

        switch (d->type) {
        case DEV_SWITCH:
            c->value = mk_label(c->card, &hd_font_text_20, UI_COL_MUTED);
            lv_obj_align(c->value, LV_ALIGN_BOTTOM_LEFT, 0, -2);
            c->sw = lv_switch_create(c->card);
            lv_obj_set_size(c->sw, 56, 30);
            lv_obj_remove_flag(c->sw, LV_OBJ_FLAG_CLICKABLE); /* Karte ist die Touch-Flaeche */
            lv_obj_align(c->sw, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
            lv_obj_add_event_cb(c->card, switch_card_clicked, LV_EVENT_CLICKED, c);
            break;

        case DEV_DIMMER:
            c->slider = lv_slider_create(c->card);
            lv_slider_set_range(c->slider, 0, 100);
            lv_obj_set_size(c->slider, 82, 12);
            lv_obj_align(c->slider, LV_ALIGN_BOTTOM_LEFT, 6, -9);
            lv_obj_set_ext_click_area(c->slider, 14);
            lv_obj_remove_flag(c->slider, LV_OBJ_FLAG_SCROLL_CHAIN_VER);
            lv_obj_set_style_bg_color(c->slider, UI_COL_TRACK, LV_PART_MAIN);
            lv_obj_set_style_bg_color(c->slider, UI_COL_ACCENT, LV_PART_INDICATOR);
            lv_obj_set_style_bg_color(c->slider, UI_COL_TEXT, LV_PART_KNOB);
            lv_obj_add_event_cb(c->slider, slider_changed, LV_EVENT_VALUE_CHANGED, c);
            lv_obj_add_event_cb(c->slider, slider_changed, LV_EVENT_RELEASED, c);

            c->value = mk_label(c->card, &hd_font_text_14, UI_COL_MUTED);
            lv_obj_align(c->value, LV_ALIGN_BOTTOM_LEFT, 104, -6);

            c->sw = lv_switch_create(c->card);
            lv_obj_set_size(c->sw, 56, 30);
            lv_obj_set_ext_click_area(c->sw, 8);
            lv_obj_align(c->sw, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
            lv_obj_add_event_cb(c->sw, dimmer_switch_clicked, LV_EVENT_CLICKED, c);
            lv_obj_add_event_cb(c->card, dimmer_card_clicked, LV_EVENT_CLICKED, c);
            break;

        case DEV_TEMPERATURE:
            lv_obj_remove_flag(c->card, LV_OBJ_FLAG_CLICKABLE);
            c->value = mk_label(c->card, &hd_font_text_32, UI_COL_TEMP);
            lv_obj_align(c->value, LV_ALIGN_BOTTOM_LEFT, 0, 4);
            break;
        }
        card_refresh(c);
    }
    return p;
}
