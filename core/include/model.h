#ifndef MODEL_H
#define MODEL_H

#include <stddef.h>
#include "knx_types.h"

/*
 * Statisches Haus-Modell (Raeume + Geraete). Wird spaeter aus dem ETS-Export
 * (.knxproj) generiert; fuer das Mockup handgeschrieben in config/house_config.c.
 * Geraete-ID == Index in house_cfg_t.devices.
 */

typedef enum {
    DEV_SWITCH = 0,  /* Schaltaktor / Lampe an-aus        (sw, sw_st)               */
    DEV_DIMMER,      /* Dimmer                            (sw, sw_st, val, val_st)  */
    DEV_TEMPERATURE, /* Temperatursensor, nur lesend      (val_st)                  */
} dev_type_t;

typedef enum {
    ICON_HOME = 0,
    ICON_COUCH,
    ICON_KITCHEN,
    ICON_BED,
    ICON_BATH,
    ICON_OFFICE,
    ICON_DOOR,
} room_icon_t;

typedef struct {
    const char *name;
    room_icon_t icon;
} room_cfg_t;

/* Bedeutung der Adressen je Typ: siehe dev_type_t. Nicht belegt = KNX_GA_NONE. */
typedef struct {
    const char *name;
    uint8_t room;      /* Index in rooms[] */
    dev_type_t type;
    knx_ga_t sw;       /* Schalten (DPT 1)                    */
    knx_ga_t sw_st;    /* Rueckmeldung Schalten (DPT 1)       */
    knx_ga_t val;      /* Wert setzen: Helligkeit (DPT 5.001) */
    knx_ga_t val_st;   /* Rueckmeldung Wert: Helligkeit (DPT 5.001) bzw. Temperatur (DPT 9) */
    /* Startwert fuer Geraete ohne Busanbindung (Demo) bzw. bis zur ersten Rueckmeldung */
    int16_t initial;   /* Switch/Dimmer: Helligkeit in %, Temperatur: Zehntelgrad */
} device_cfg_t;

typedef struct {
    bool known;        /* false = noch kein Wert vom Bus erhalten */
    bool on;
    uint8_t level;     /* 0..100 % (Dimmer) */
    int16_t temp_x10;  /* Zehntelgrad Celsius */
} device_state_t;

typedef struct {
    const room_cfg_t *rooms;
    size_t room_count;
    const device_cfg_t *devices;
    size_t device_count;
} house_cfg_t;

#define MAX_DEVICES 64

const house_cfg_t *house_config(void);

#endif
