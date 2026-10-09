#!/usr/bin/env bash
# Erzeugt die LVGL-Bitmap-Fonts in ui/fonts/ (Latin-1 Text + FontAwesome-Icons).
# Aufruf: tools/gen_fonts.sh <pfad-zu-lvgl-source>
set -euo pipefail
LVGL="${1:?Pfad zum LVGL-Source angeben (enthaelt scripts/built_in_font)}"
OUT="$(cd "$(dirname "$0")/.." && pwd)/ui/fonts"
TTF="$LVGL/scripts/built_in_font/Montserrat-Medium.ttf"
FA="$LVGL/scripts/built_in_font/FontAwesome5-Solid+Brands+Regular.woff"
CONV="npx --yes lv_font_conv@1.5.3"

# Text: ASCII + Latin-1 (Umlaute, ss, Grad-Zeichen)
for size in 14 18 20 32; do
  $CONV --font "$TTF" --size $size --bpp 4 --range 0x20-0x7F,0xA0-0xFF \
        --format lvgl --lv-include lvgl.h --no-compress \
        --lv-font-name hd_font_text_$size -o "$OUT/hd_font_text_$size.c"
done

# Icons (FontAwesome 5 solid) -- Codepoints muessen zu ui/ui_icons.h passen
ICONS=0xf015,0xf4b8,0xf2e7,0xf236,0xf2cd,0xf108,0xf52b,0xf0eb,0xf2c9,0xf013,0xf1eb,0xf053,0xf054,0xf011,0xf185,0xf071,0xf0c1,0xf127
for size in 20 32 48; do
  $CONV --font "$FA" --size $size --bpp 4 --symbols "" -r $ICONS \
        --format lvgl --lv-include lvgl.h --no-compress \
        --lv-font-name hd_font_icons_$size -o "$OUT/hd_font_icons_$size.c"
done
