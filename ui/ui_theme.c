#include "ui_theme.h"

static lv_style_t st_card;
static lv_style_t st_card_pr;

void ui_theme_init(lv_display_t *disp)
{
    lv_theme_t *th = lv_theme_default_init(disp, UI_COL_ACCENT, UI_COL_TEMP, true, &hd_font_text_14);
    lv_display_set_theme(disp, th);

    lv_style_init(&st_card);
    lv_style_set_bg_color(&st_card, UI_COL_CARD);
    lv_style_set_bg_opa(&st_card, LV_OPA_COVER);
    lv_style_set_radius(&st_card, 12);
    lv_style_set_border_width(&st_card, 0);
    lv_style_set_pad_all(&st_card, 10);
    lv_style_set_text_color(&st_card, UI_COL_TEXT);
    lv_style_set_shadow_width(&st_card, 0);
    lv_style_init(&st_card_pr);
    lv_style_set_bg_color(&st_card_pr, UI_COL_CARD_PR);
}

lv_obj_t *ui_card_create(lv_obj_t *parent, int32_t w, int32_t h)
{
    lv_obj_t *c = lv_obj_create(parent);
    lv_obj_remove_style_all(c);
    lv_obj_add_style(c, &st_card, 0);
    lv_obj_add_style(c, &st_card_pr, LV_STATE_PRESSED);
    lv_obj_set_size(c, w, h);
    lv_obj_remove_flag(c, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);
    return c;
}

void ui_label_oneline(lv_obj_t *label, const lv_font_t *font, int32_t width)
{
    lv_obj_set_style_text_font(label, font, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    lv_obj_set_size(label, width, lv_font_get_line_height(font));
}
