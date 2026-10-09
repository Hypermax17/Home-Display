#include "ui_nav.h"
#include <stdio.h>

#define SLIDER_SEND_MS 120

typedef struct {
    uint16_t dev;
    lv_obj_t *btn;
    lv_obj_t *icon;
    lv_obj_t *slider;
    lv_obj_t *value;
    uint32_t last_send;
} dev_priv_t;

static void refresh(dev_priv_t *d)
{
    const device_state_t *s = ui_state_dev(d->dev);
    char buf[16];
    snprintf(buf, sizeof(buf), "%d %%", s->level);
    lv_label_set_text(d->value, buf);
    if (!lv_slider_is_dragged(d->slider)) {
        lv_slider_set_value(d->slider, s->level, LV_ANIM_OFF);
    }
    lv_obj_set_style_bg_color(d->btn, s->on ? UI_COL_ACCENT : UI_COL_CARD, 0);
    lv_obj_set_style_text_color(d->icon, s->on ? lv_color_hex(0x1A1200) : UI_COL_MUTED, 0);
    lv_obj_set_style_text_color(d->value, s->on ? UI_COL_TEXT : UI_COL_MUTED, 0);
}

static void on_state(ui_page_t *p, uint16_t dev)
{
    dev_priv_t *d = p->priv;
    if (d->dev == dev) {
        refresh(d);
    }
}

static void btn_clicked(lv_event_t *e)
{
    dev_priv_t *d = lv_event_get_user_data(e);
    ui_cmd_set_switch(d->dev, !ui_state_dev(d->dev)->on);
}

static void slider_changed(lv_event_t *e)
{
    dev_priv_t *d = lv_event_get_user_data(e);
    int v = lv_slider_get_value(lv_event_get_target(e));
    char buf[16];
    snprintf(buf, sizeof(buf), "%d %%", v);
    lv_label_set_text(d->value, buf);

    uint32_t now = lv_tick_get();
    if (lv_event_get_code(e) == LV_EVENT_RELEASED || (now - d->last_send) >= SLIDER_SEND_MS) {
        d->last_send = now;
        ui_cmd_set_level(d->dev, (uint8_t)v);
    }
}

ui_page_t *ui_page_device_create(uint16_t dev)
{
    const device_cfg_t *cfg = &house_config()->devices[dev];
    ui_page_t *p = ui_page_create(cfg->name, true, false);
    dev_priv_t *d = ui_page_alloc_priv(p, sizeof(*d));
    d->dev = dev;
    p->on_state = on_state;

    lv_obj_t *c = p->content;
    lv_obj_remove_flag(c, LV_OBJ_FLAG_SCROLLABLE);

    /* links: grosser Ein/Aus-Knopf */
    d->btn = ui_card_create(c, 120, 120);
    lv_obj_set_style_radius(d->btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_align(d->btn, LV_ALIGN_LEFT_MID, 24, 0);
    lv_obj_add_event_cb(d->btn, btn_clicked, LV_EVENT_CLICKED, d);
    d->icon = lv_label_create(d->btn);
    lv_label_set_text(d->icon, HD_ICON_POWER);
    lv_obj_set_style_text_font(d->icon, &hd_font_icons_48, 0);
    lv_obj_center(d->icon);

    /* rechts: Helligkeit */
    d->value = lv_label_create(c);
    lv_obj_set_style_text_font(d->value, &hd_font_text_32, 0);
    lv_obj_align(d->value, LV_ALIGN_TOP_RIGHT, -60, 34);

    d->slider = lv_slider_create(c);
    lv_slider_set_range(d->slider, 0, 100);
    lv_obj_set_size(d->slider, 230, 26);
    lv_obj_align(d->slider, LV_ALIGN_RIGHT_MID, -36, 28);
    lv_obj_set_ext_click_area(d->slider, 14);
    lv_obj_set_style_bg_color(d->slider, UI_COL_TRACK, LV_PART_MAIN);
    lv_obj_set_style_bg_color(d->slider, UI_COL_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(d->slider, UI_COL_TEXT, LV_PART_KNOB);
    lv_obj_set_style_pad_all(d->slider, 4, LV_PART_KNOB);
    lv_obj_add_event_cb(d->slider, slider_changed, LV_EVENT_VALUE_CHANGED, d);
    lv_obj_add_event_cb(d->slider, slider_changed, LV_EVENT_RELEASED, d);

    refresh(d);
    return p;
}
