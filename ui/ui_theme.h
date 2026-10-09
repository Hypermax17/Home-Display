#ifndef UI_THEME_H
#define UI_THEME_H

#include "lvgl.h"
#include "ui_icons.h"

LV_FONT_DECLARE(hd_font_text_14);
LV_FONT_DECLARE(hd_font_text_18);
LV_FONT_DECLARE(hd_font_text_20);
LV_FONT_DECLARE(hd_font_text_32);
LV_FONT_DECLARE(hd_font_icons_20);
LV_FONT_DECLARE(hd_font_icons_32);
LV_FONT_DECLARE(hd_font_icons_48);

#define UI_SCREEN_W 480
#define UI_SCREEN_H 272
#define UI_HEADER_H 44
#define UI_PAD 8

#define UI_COL_BG        lv_color_hex(0x0F1318)
#define UI_COL_HEADER    lv_color_hex(0x151B22)
#define UI_COL_CARD      lv_color_hex(0x1B2129)
#define UI_COL_CARD_PR   lv_color_hex(0x28313C)
#define UI_COL_TRACK     lv_color_hex(0x2E3946)
#define UI_COL_TEXT      lv_color_hex(0xE8EDF2)
#define UI_COL_MUTED     lv_color_hex(0x8A97A6)
#define UI_COL_ACCENT    lv_color_hex(0xFFB020) /* Licht an */
#define UI_COL_TEMP      lv_color_hex(0x4FC3F7)
#define UI_COL_OK        lv_color_hex(0x3DDC84)
#define UI_COL_ERR       lv_color_hex(0xFF5252)

/* Einzeiliges Label fester Breite, ueberlanger Text wird mit "..." gekuerzt */
void ui_label_oneline(lv_obj_t *label, const lv_font_t *font, int32_t width);

void ui_theme_init(lv_display_t *disp);

/* Kachel mit Standard-Styling (Hintergrund, Rundung, gedrueckt-Zustand), anklickbar */
lv_obj_t *ui_card_create(lv_obj_t *parent, int32_t w, int32_t h);

#endif
