#ifndef DPT_H
#define DPT_H

#include "knx_types.h"

/* DPT 1.xxx (1 Bit) */
void dpt1_encode(knx_telegram_t *t, bool v);
bool dpt1_decode(const knx_telegram_t *t, bool *v);

/* DPT 5.001 (Prozent 0..100 <-> 0..255) */
void dpt5_encode_percent(knx_telegram_t *t, uint8_t percent);
bool dpt5_decode_percent(const knx_telegram_t *t, uint8_t *percent);

/* DPT 9.xxx (16-Bit-Gleitkomma), Wert in Zehntelgrad: 215 = 21,5 */
void dpt9_encode_x10(knx_telegram_t *t, int16_t x10);
bool dpt9_decode_x10(const knx_telegram_t *t, int16_t *x10);

#endif
