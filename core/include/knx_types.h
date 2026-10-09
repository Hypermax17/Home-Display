#ifndef KNX_TYPES_H
#define KNX_TYPES_H

#include <stdbool.h>
#include <stdint.h>

/* Gruppenadresse (3-stufig: Haupt/Mittel/Unter) als 16 Bit: HHHHH MMM UUUUUUUU */
typedef uint16_t knx_ga_t;

#define KNX_GA(main, mid, sub) \
    ((knx_ga_t)((((main) & 0x1F) << 11) | (((mid) & 0x07) << 8) | ((sub) & 0xFF)))
#define KNX_GA_NONE ((knx_ga_t)0) /* 0/0/0 ist nicht als Ziel nutzbar -> "nicht belegt" */

#define KNX_GA_MAIN(ga) (((ga) >> 11) & 0x1F)
#define KNX_GA_MID(ga)  (((ga) >> 8) & 0x07)
#define KNX_GA_SUB(ga)  ((ga) & 0xFF)

typedef enum {
    KNX_APCI_READ = 0,
    KNX_APCI_RESPONSE = 1,
    KNX_APCI_WRITE = 2,
} knx_apci_t;

#define KNX_MAX_DATA 14

/*
 * Ein Gruppentelegramm. len == 0: Wert (max. 6 Bit) steckt in data[0]
 * (z.B. DPT 1). len >= 1: len Datenbytes in data[] (z.B. DPT 5 = 1, DPT 9 = 2).
 */
typedef struct {
    knx_ga_t ga;
    knx_apci_t apci;
    uint8_t len;
    uint8_t data[KNX_MAX_DATA];
} knx_telegram_t;

typedef enum {
    LINK_DOWN = 0,
    LINK_CONNECTING,
    LINK_UP,
} link_state_t;

#endif
