#include "ui_nav.h"

static const char *g_gateway = "-";
static const char *g_version = "-";

void ui_set_info(const char *gateway, const char *version)
{
    g_gateway = gateway ? gateway : "-";
    g_version = version ? version : "-";
}
const char *ui_info_gateway(void) { return g_gateway; }
const char *ui_info_version(void) { return g_version; }

typedef struct {
    lv_obj_t *link_val;
} settings_priv_t;

static void refresh_link(settings_priv_t *s)
{
    link_state_t l = ui_state_link();
    const char *txt = l == LINK_UP ? "Verbunden" : (l == LINK_CONNECTING ? "Verbinde ..." : "Getrennt");
    lv_color_t col = l == LINK_UP ? UI_COL_OK : (l == LINK_CONNECTING ? UI_COL_ACCENT : UI_COL_ERR);
    lv_label_set_text(s->link_val, txt);
    lv_obj_set_style_text_color(s->link_val, col, 0);
}

static void on_link(ui_page_t *p)
{
    refresh_link(p->priv);
}

static lv_obj_t *row(lv_obj_t *parent, const char *key, const char *val)
{
    lv_obj_t *r = ui_card_create(parent, UI_SCREEN_W - 2 * UI_PAD, 48);
    lv_obj_remove_flag(r, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *k = lv_label_create(r);
    lv_label_set_text(k, key);
    lv_obj_set_style_text_font(k, &hd_font_text_20, 0);
    lv_obj_align(k, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_t *v = lv_label_create(r);
    lv_label_set_text(v, val);
    lv_obj_set_style_text_font(v, &hd_font_text_20, 0);
    lv_obj_set_style_text_color(v, UI_COL_MUTED, 0);
    lv_obj_align(v, LV_ALIGN_RIGHT_MID, 0, 0);
    return v;
}

ui_page_t *ui_page_settings_create(void)
{
    ui_page_t *p = ui_page_create("Einstellungen", true, false);
    settings_priv_t *s = ui_page_alloc_priv(p, sizeof(*s));
    p->on_link = on_link;

    lv_obj_set_flex_flow(p->content, LV_FLEX_FLOW_COLUMN);
    s->link_val = row(p->content, "KNX-Verbindung", "");
    row(p->content, "Gateway", g_gateway);
    row(p->content, "Version", g_version);
    refresh_link(s);
    return p;
}
