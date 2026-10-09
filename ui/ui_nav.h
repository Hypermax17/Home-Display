#ifndef UI_NAV_H
#define UI_NAV_H

#include "ui_theme.h"
#include "ui_state.h"

/*
 * Seitenverwaltung: jede Seite ist ein eigener LVGL-Screen mit gemeinsamem Header
 * (Zurueck, Titel, Verbindungsstatus). Navigation = Stack mit Slide-Animation:
 *
 *   Home (Raeume) -> Raum (Geraete) -> Geraet-Detail
 *        \-> Einstellungen
 */
typedef struct ui_page {
    lv_obj_t *screen;
    lv_obj_t *content;   /* Bereich unter dem Header */
    lv_obj_t *link_icon;
    void (*on_state)(struct ui_page *p, uint16_t dev); /* optional */
    void (*on_link)(struct ui_page *p);                /* optional */
    void (*on_resume)(struct ui_page *p);              /* optional: Seite wird wieder sichtbar (Zustand komplett neu zeichnen) */
    void *priv;                                        /* wird mit der Seite freigegeben */
} ui_page_t;

/* Seite mit Header anlegen. settings_btn: Zahnrad im Header (nur Startseite). */
ui_page_t *ui_page_create(const char *title, bool back_btn, bool settings_btn);
void *ui_page_alloc_priv(ui_page_t *p, size_t size);

void ui_nav_set_root(ui_page_t *p);
void ui_nav_push(ui_page_t *p);
void ui_nav_back(void);
ui_page_t *ui_nav_top(void);

/* Ereignisse der Datenschicht an die sichtbare Seite weiterreichen */
void ui_nav_notify_state(uint16_t dev);
void ui_nav_notify_link(void);

void ui_toast(const char *text);

/* Seiten */
ui_page_t *ui_page_home_create(void);
ui_page_t *ui_page_room_create(uint8_t room);
ui_page_t *ui_page_device_create(uint16_t dev);
ui_page_t *ui_page_settings_create(void);
void ui_set_info(const char *gateway, const char *version);
const char *ui_info_gateway(void);
const char *ui_info_version(void);

#endif
