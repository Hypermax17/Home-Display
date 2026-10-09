#include "dpt.h"

void dpt1_encode(knx_telegram_t *t, bool v)
{
    t->len = 0;
    t->data[0] = v ? 1 : 0;
}

bool dpt1_decode(const knx_telegram_t *t, bool *v)
{
    if (t->len != 0) {
        return false;
    }
    *v = (t->data[0] & 1) != 0;
    return true;
}

void dpt5_encode_percent(knx_telegram_t *t, uint8_t percent)
{
    if (percent > 100) {
        percent = 100;
    }
    t->len = 1;
    t->data[0] = (uint8_t)((percent * 255u + 50u) / 100u);
}

bool dpt5_decode_percent(const knx_telegram_t *t, uint8_t *percent)
{
    if (t->len != 1) {
        return false;
    }
    *percent = (uint8_t)((t->data[0] * 100u + 127u) / 255u);
    return true;
}

void dpt9_encode_x10(knx_telegram_t *t, int16_t x10)
{
    /* Wert * 100 = mantisse * 2^exp, mantisse 12 Bit vorzeichenbehaftet */
    int32_t m = (int32_t)x10 * 10;
    uint8_t e = 0;
    while ((m > 2047 || m < -2048) && e < 15) {
        m /= 2;
        e++;
    }
    uint16_t raw = (uint16_t)(((m < 0) ? 0x8000u : 0u) | ((uint16_t)e << 11) | ((uint16_t)m & 0x07FFu));
    t->len = 2;
    t->data[0] = (uint8_t)(raw >> 8);
    t->data[1] = (uint8_t)(raw & 0xFF);
}

bool dpt9_decode_x10(const knx_telegram_t *t, int16_t *x10)
{
    if (t->len != 2) {
        return false;
    }
    uint16_t raw = (uint16_t)((t->data[0] << 8) | t->data[1]);
    int32_t m = raw & 0x07FF;
    if (raw & 0x8000) {
        m -= 2048;
    }
    uint8_t e = (raw >> 11) & 0x0F;
    int32_t v100 = m * (1 << e); /* Wert * 100 */
    /* auf Zehntel runden (weg von 0) */
    int32_t v10 = (v100 >= 0) ? (v100 + 5) / 10 : (v100 - 5) / 10;
    *x10 = (int16_t)v10;
    return true;
}
