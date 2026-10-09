#include "ui_nav.h"

#define NAV_DEPTH 6
#define ANIM_MS 220

static ui_page_t *g_stack[NAV_DEPTH];
static int g_depth;
static uint32_t g_busy_until;

static bool nav_busy(void)
{
    return (int32_t)(lv_tick_get() - g_busy_until) < 0;
}

static lv_color_t link_color(link_state_t l)
{
    switch (l) {
    case LINK_UP:         return UI_COL_OK;
    case LINK_CONNECTING: return UI_COL_ACCENT;
    default:              return UI_COL_ERR;
    }
}

static void update_link_icon(ui_page_t *p)
{
    link_state_t l = ui_state_link();
    lv_label_set_text(p->link_icon, l == LINK_UP ? HD_ICON_LINK : HD_ICON_UNLINK);
    lv_obj_set_style_text_color(p->link_icon, link_color(l), 0);
}

static void on_back_clicked(lv_event_t *e)
{
    (void)e;
    ui_nav_back();
}

static void on_settings_clicked(lv_event_t *e)
{
    (void)e;
    ui_nav_push(ui_page_settings_create());
}

static void on_screen_delete(lv_event_t *e)
{
    ui_page_t *p = lv_event_get_user_data(e);
    lv_free(p->priv);
    lv_free(p);
}

static lv_obj_t *header_button(lv_obj_t *parent, const char *icon, lv_event_cb_t cb)
{
    lv_obj_t *b = lv_obj_create(parent);
    lv_obj_remove_style_all(b);
    lv_obj_set_size(b, 56, UI_HEADER_H);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(b, LV_OPA_20, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(b, lv_color_white(), LV_STATE_PRESSED);
    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text(l, icon);
    lv_obj_set_style_text_font(l, &hd_font_icons_20, 0);
    lv_obj_set_style_text_color(l, UI_COL_TEXT, 0);
    lv_obj_center(l);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, NULL);
    return b;
}

ui_page_t *ui_page_create(const char *title, bool back_btn, bool settings_btn)
{
    ui_page_t *p = lv_malloc_zeroed(sizeof(*p));
    LV_ASSERT_MALLOC(p);

    p->screen = lv_obj_create(NULL);
    lv_obj_remove_flag(p->screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(p->screen, UI_COL_BG, 0);
    lv_obj_set_style_bg_opa(p->screen, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(p->screen, 0, 0);
    lv_obj_add_event_cb(p->screen, on_screen_delete, LV_EVENT_DELETE, p);

    /* Header */
    lv_obj_t *hdr = lv_obj_create(p->screen);
    lv_obj_remove_style_all(hdr);
    lv_obj_set_size(hdr, UI_SCREEN_W, UI_HEADER_H);
    lv_obj_set_style_bg_color(hdr, UI_COL_HEADER, 0);
    lv_obj_set_style_bg_opa(hdr, LV_OPA_COVER, 0);
    lv_obj_remove_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);

    if (back_btn) {
        lv_obj_t *b = header_button(hdr, HD_ICON_LEFT, on_back_clicked);
        lv_obj_align(b, LV_ALIGN_LEFT_MID, 0, 0);
    }

    lv_obj_t *t = lv_label_create(hdr);
    lv_label_set_text(t, title);
    ui_label_oneline(t, &hd_font_text_20, 300);
    lv_obj_set_style_text_color(t, UI_COL_TEXT, 0);
    lv_obj_align(t, LV_ALIGN_LEFT_MID, back_btn ? 60 : 16, 0);

    int x_right = 0;
    if (settings_btn) {
        lv_obj_t *s = header_button(hdr, HD_ICON_COG, on_settings_clicked);
        lv_obj_align(s, LV_ALIGN_RIGHT_MID, 0, 0);
        x_right = -56;
    }
    p->link_icon = lv_label_create(hdr);
    lv_obj_set_style_text_font(p->link_icon, &hd_font_icons_20, 0);
    lv_obj_align(p->link_icon, LV_ALIGN_RIGHT_MID, x_right - 16, 0);
    update_link_icon(p);

    /* Inhalt */
    p->content = lv_obj_create(p->screen);
    lv_obj_remove_style_all(p->content);
    lv_obj_set_pos(p->content, 0, UI_HEADER_H);
    lv_obj_set_size(p->content, UI_SCREEN_W, UI_SCREEN_H - UI_HEADER_H);
    lv_obj_set_style_pad_all(p->content, UI_PAD, 0);
    lv_obj_set_style_pad_gap(p->content, UI_PAD, 0);
    lv_obj_set_scrollbar_mode(p->content, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_text_color(p->content, UI_COL_TEXT, 0);
    return p;
}

void *ui_page_alloc_priv(ui_page_t *p, size_t size)
{
    p->priv = lv_malloc_zeroed(size);
    LV_ASSERT_MALLOC(p->priv);
    return p->priv;
}

void ui_nav_set_root(ui_page_t *p)
{
    g_stack[0] = p;
    g_depth = 1;
    lv_screen_load(p->screen);
}

void ui_nav_push(ui_page_t *p)
{
    if (nav_busy() || g_depth >= NAV_DEPTH) {
        lv_obj_delete(p->screen);
        return;
    }
    g_stack[g_depth++] = p;
    g_busy_until = lv_tick_get() + ANIM_MS + 30;
    lv_screen_load_anim(p->screen, LV_SCR_LOAD_ANIM_MOVE_LEFT, ANIM_MS, 0, false);
}

void ui_nav_back(void)
{
    if (nav_busy() || g_depth < 2) {
        return;
    }
    g_depth--; /* oberste Seite verlassen; Screen wird nach der Animation geloescht */
    ui_page_t *prev = g_stack[g_depth - 1];
    g_busy_until = lv_tick_get() + ANIM_MS + 30;
    lv_screen_load_anim(prev->screen, LV_SCR_LOAD_ANIM_MOVE_RIGHT, ANIM_MS, 0, true);
    /* Waehrend die Seite verdeckt war, bekam sie keine Events -> jetzt nachziehen */
    update_link_icon(prev);
    if (prev->on_resume) {
        prev->on_resume(prev);
    }
}

ui_page_t *ui_nav_top(void)
{
    return g_depth > 0 ? g_stack[g_depth - 1] : NULL;
}

void ui_nav_notify_state(uint16_t dev)
{
    ui_page_t *p = ui_nav_top();
    if (p && p->on_state) {
        p->on_state(p, dev);
    }
}

void ui_nav_notify_link(void)
{
    ui_page_t *p = ui_nav_top();
    if (!p) {
        return;
    }
    update_link_icon(p);
    if (p->on_link) {
        p->on_link(p);
    }
}

static void toast_anim_exec(void *obj, int32_t v)
{
    lv_obj_set_style_opa(obj, (lv_opa_t)v, 0);
}

void ui_toast(const char *text)
{
    lv_obj_t *t = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(t);
    lv_obj_set_style_bg_color(t, UI_COL_ERR, 0);
    lv_obj_set_style_bg_opa(t, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(t, 8, 0);
    lv_obj_set_style_pad_hor(t, 14, 0);
    lv_obj_set_style_pad_ver(t, 8, 0);
    lv_obj_set_size(t, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_remove_flag(t, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *l = lv_label_create(t);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_font(l, &hd_font_text_14, 0);
    lv_obj_set_style_text_color(l, lv_color_white(), 0);
    lv_obj_align(t, LV_ALIGN_BOTTOM_MID, 0, -12);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, t);
    lv_anim_set_exec_cb(&a, toast_anim_exec);
    lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_delay(&a, 2200);
    lv_anim_set_duration(&a, 400);
    lv_anim_start(&a);
    lv_obj_delete_delayed(t, 2650);
}
