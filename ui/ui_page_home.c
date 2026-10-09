#include "ui_nav.h"
#include <stdio.h>

typedef struct {
    uint8_t room;
    lv_obj_t *icon;
    lv_obj_t *temp;
    lv_obj_t *status;
} tile_t;

typedef struct {
    tile_t tiles[16];
    size_t n;
} home_priv_t;

static const char *room_icon(room_icon_t i)
{
    switch (i) {
    case ICON_COUCH:   return HD_ICON_COUCH;
    case ICON_KITCHEN: return HD_ICON_KITCHEN;
    case ICON_BED:     return HD_ICON_BED;
    case ICON_BATH:    return HD_ICON_BATH;
    case ICON_OFFICE:  return HD_ICON_OFFICE;
    case ICON_DOOR:    return HD_ICON_DOOR;
    default:           return HD_ICON_HOME;
    }
}

static void tile_refresh(tile_t *t)
{
    int on = ui_room_lights_on(t->room);
    int total = ui_room_light_count(t->room);
    char buf[48];

    if (on == 0) {
        snprintf(buf, sizeof(buf), "Licht aus");
    } else if (on == 1) {
        snprintf(buf, sizeof(buf), "1 Licht an");
    } else {
        snprintf(buf, sizeof(buf), "%d Lichter an", on);
    }
    if (total == 0) {
        buf[0] = 0;
    }
    lv_label_set_text(t->status, buf);
    lv_obj_set_style_text_color(t->status, on ? UI_COL_ACCENT : UI_COL_MUTED, 0);
    lv_obj_set_style_text_color(t->icon, on ? UI_COL_ACCENT : UI_COL_MUTED, 0);

    int16_t x10;
    if (ui_room_temperature(t->room, &x10)) {
        ui_format_temp(buf, sizeof(buf), x10);
        lv_label_set_text(t->temp, buf);
    } else {
        lv_label_set_text(t->temp, "");
    }
}

static void tile_clicked(lv_event_t *e)
{
    uint8_t room = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    ui_nav_push(ui_page_room_create(room));
}

static void home_on_state(ui_page_t *p, uint16_t dev)
{
    home_priv_t *h = p->priv;
    uint8_t room = house_config()->devices[dev].room;
    for (size_t i = 0; i < h->n; i++) {
        if (h->tiles[i].room == room) {
            tile_refresh(&h->tiles[i]);
        }
    }
}

static void home_on_resume(ui_page_t *p)
{
    home_priv_t *h = p->priv;
    for (size_t i = 0; i < h->n; i++) {
        tile_refresh(&h->tiles[i]);
    }
}

ui_page_t *ui_page_home_create(void)
{
    ui_page_t *p = ui_page_create("Räume", false, true);
    home_priv_t *h = ui_page_alloc_priv(p, sizeof(*h));
    const house_cfg_t *cfg = house_config();

    lv_obj_set_flex_flow(p->content, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_add_flag(p->content, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(p->content, LV_DIR_VER);
    p->on_state = home_on_state;
    p->on_resume = home_on_resume;

    const int32_t tile_w = (UI_SCREEN_W - 2 * UI_PAD - 2 * UI_PAD) / 3;
    const int32_t tile_h = (UI_SCREEN_H - UI_HEADER_H - 2 * UI_PAD - UI_PAD) / 2;

    for (size_t r = 0; r < cfg->room_count && h->n < 16; r++) {
        tile_t *t = &h->tiles[h->n++];
        t->room = (uint8_t)r;

        lv_obj_t *card = ui_card_create(p->content, tile_w, tile_h);
        lv_obj_add_event_cb(card, tile_clicked, LV_EVENT_CLICKED, (void *)(uintptr_t)r);

        t->icon = lv_label_create(card);
        lv_label_set_text(t->icon, room_icon(cfg->rooms[r].icon));
        lv_obj_set_style_text_font(t->icon, &hd_font_icons_32, 0);
        lv_obj_align(t->icon, LV_ALIGN_TOP_LEFT, 0, 0);

        t->temp = lv_label_create(card);
        lv_obj_set_style_text_font(t->temp, &hd_font_text_14, 0);
        lv_obj_set_style_text_color(t->temp, UI_COL_TEMP, 0);
        lv_obj_align(t->temp, LV_ALIGN_TOP_RIGHT, 0, 2);

        lv_obj_t *name = lv_label_create(card);
        lv_label_set_text(name, cfg->rooms[r].name);
        ui_label_oneline(name, &hd_font_text_18, tile_w - 20);
        lv_obj_align(name, LV_ALIGN_BOTTOM_LEFT, 0, -18);

        t->status = lv_label_create(card);
        lv_obj_set_style_text_font(t->status, &hd_font_text_14, 0);
        lv_obj_align(t->status, LV_ALIGN_BOTTOM_LEFT, 0, 0);

        tile_refresh(t);
    }
    return p;
}
