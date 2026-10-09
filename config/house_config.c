#include "model.h"

/*
 * Mockup-Konfiguration. Spaeter wird diese Datei aus dem ETS-Projekt generiert.
 *
 * PROOF OF CONCEPT: Nur "Wohnzimmer / Deckenlicht" ist mit dem Bus verbunden.
 * Passe die beiden Adressen an deine Installation an. Alle anderen Geraete haben
 * keine Gruppenadressen und verhalten sich als lokale Demo (Zustand nur im Display).
 */
#define POC_LAMP_SWITCH_GA   KNX_GA(1, 0, 1) /* Schalten (DPT 1)                      */
#define POC_LAMP_STATUS_GA   KNX_GA(1, 0, 2) /* Rueckmeldung (DPT 1), KNX_GA_NONE wenn keine */

static const room_cfg_t rooms[] = {
    { "Wohnzimmer",   ICON_COUCH   },
    { "Küche",        ICON_KITCHEN },
    { "Schlafzimmer", ICON_BED     },
    { "Bad",          ICON_BATH    },
    { "Büro",         ICON_OFFICE  },
    { "Flur",         ICON_DOOR    },
};

#define N KNX_GA_NONE
static const device_cfg_t devices[] = {
    /* name              raum type              sw                    sw_st               val val_st initial */
    { "Deckenlicht",     0, DEV_SWITCH,      POC_LAMP_SWITCH_GA, POC_LAMP_STATUS_GA, N,  N,  0   },
    { "Stehlampe",       0, DEV_DIMMER,      N,                  N,                  N,  N,  60  },
    { "Temperatur",      0, DEV_TEMPERATURE, N,                  N,                  N,  N,  214 },

    { "Deckenlicht",     1, DEV_SWITCH,      N,                  N,                  N,  N,  100 },
    { "Arbeitsplatte",   1, DEV_DIMMER,      N,                  N,                  N,  N,  0   },
    { "Temperatur",      1, DEV_TEMPERATURE, N,                  N,                  N,  N,  226 },

    { "Deckenlicht",     2, DEV_SWITCH,      N,                  N,                  N,  N,  0   },
    { "Nachttisch links",2, DEV_DIMMER,      N,                  N,                  N,  N,  25  },
    { "Nachttisch rechts",2, DEV_DIMMER,     N,                  N,                  N,  N,  0   },
    { "Temperatur",      2, DEV_TEMPERATURE, N,                  N,                  N,  N,  186 },

    { "Deckenlicht",     3, DEV_SWITCH,      N,                  N,                  N,  N,  0   },
    { "Spiegellicht",    3, DEV_DIMMER,      N,                  N,                  N,  N,  80  },
    { "Temperatur",      3, DEV_TEMPERATURE, N,                  N,                  N,  N,  231 },

    { "Deckenlicht",     4, DEV_SWITCH,      N,                  N,                  N,  N,  100 },
    { "Schreibtisch",    4, DEV_DIMMER,      N,                  N,                  N,  N,  100 },
    { "Temperatur",      4, DEV_TEMPERATURE, N,                  N,                  N,  N,  219 },

    { "Deckenlicht",     5, DEV_SWITCH,      N,                  N,                  N,  N,  0   },
};
#undef N

static const house_cfg_t cfg = {
    rooms, sizeof(rooms) / sizeof(rooms[0]),
    devices, sizeof(devices) / sizeof(devices[0]),
};

const house_cfg_t *house_config(void)
{
    return &cfg;
}
