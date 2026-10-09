#ifndef UI_STATE_H
#define UI_STATE_H

#include "app_bus.h"


/* UI-seitiger Spiegel der Zustaende (nur aus Events der Datenschicht befuellt). */
const device_state_t *ui_state_dev(uint16_t dev);
link_state_t ui_state_link(void);
void ui_state_apply(const app_evt_t *e);

/* Kommandos an die Datenschicht */
void ui_cmd_set_switch(uint16_t dev, bool on);
void ui_cmd_set_level(uint16_t dev, uint8_t percent);
void ui_cmd_refresh(void);

/* Hilfsfunktionen auf dem Modell */
int ui_room_lights_on(uint8_t room);
int ui_room_light_count(uint8_t room);
bool ui_room_temperature(uint8_t room, int16_t *temp_x10); /* false = kein Sensor / noch kein Wert */
void ui_format_temp(char *buf, size_t n, int16_t x10);

#endif
