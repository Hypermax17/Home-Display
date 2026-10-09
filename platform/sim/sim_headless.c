#include "sim_headless.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define W 480
#define H 272

static uint16_t g_fb[W * H];          /* zuletzt gerenderter Frame (RGB565) */
static uint16_t g_buf[W * H];         /* LVGL-Zeichenpuffer (volle Groesse) */
static char g_shot_dir[256] = ".";

typedef enum { OP_WAIT, OP_TAP, OP_DRAG, OP_SHOT, OP_QUIT } op_t;
typedef struct {
    op_t op;
    int a, b, c, d;
    char name[96];
} step_t;

static step_t g_steps[256];
static int g_n, g_pos;
static uint32_t g_t0;      /* Startzeit des aktuellen Schritts */
static bool g_started;
static bool g_done;

/* virtueller Zeiger */
static bool g_dirty;
static bool g_pressed;
static int g_px, g_py;

static void flush_cb(lv_display_t *d, const lv_area_t *a, uint8_t *px)
{
    (void)a;
    memcpy(g_fb, px, sizeof(g_fb));
    g_dirty = true;
    lv_display_flush_ready(d);
}

static void pointer_read(lv_indev_t *i, lv_indev_data_t *data)
{
    (void)i;
    data->point.x = g_px;
    data->point.y = g_py;
    data->state = g_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

static void write_ppm(const char *name)
{
    char path[400];
    snprintf(path, sizeof(path), "%s/%s", g_shot_dir, name);
    FILE *f = fopen(path, "wb");
    if (!f) {
        perror(path);
        return;
    }
    fprintf(f, "P6\n%d %d\n255\n", W, H);
    for (int i = 0; i < W * H; i++) {
        uint16_t c = g_fb[i];
        uint8_t rgb[3] = {
            (uint8_t)(((c >> 11) & 0x1F) * 255 / 31),
            (uint8_t)(((c >> 5) & 0x3F) * 255 / 63),
            (uint8_t)((c & 0x1F) * 255 / 31),
        };
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
    printf("Screenshot: %s\n", path);
}

static bool load_script(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) {
        perror(path);
        return false;
    }
    char line[200];
    while (fgets(line, sizeof(line), f) && g_n < 256) {
        step_t *s = &g_steps[g_n];
        memset(s, 0, sizeof(*s));
        if (sscanf(line, "wait %d", &s->a) == 1) s->op = OP_WAIT;
        else if (sscanf(line, "tap %d %d", &s->a, &s->b) == 2) s->op = OP_TAP;
        else if (sscanf(line, "drag %d %d %d %d", &s->a, &s->b, &s->c, &s->d) == 4) s->op = OP_DRAG;
        else if (sscanf(line, "shot %95s", s->name) == 1) s->op = OP_SHOT;
        else if (strncmp(line, "quit", 4) == 0) s->op = OP_QUIT;
        else continue;
        g_n++;
    }
    fclose(f);
    return true;
}

lv_display_t *sim_headless_create(const char *shot_dir, const char *script_path)
{
    if (shot_dir) {
        snprintf(g_shot_dir, sizeof(g_shot_dir), "%s", shot_dir);
    }
    if (script_path && !load_script(script_path)) {
        return NULL;
    }
    lv_display_t *d = lv_display_create(W, H);
    lv_display_set_color_format(d, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(d, g_buf, NULL, sizeof(g_buf), LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(d, flush_cb);

    lv_indev_t *in = lv_indev_create();
    lv_indev_set_type(in, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(in, pointer_read);
    return d;
}

#define TAP_MS 90
#define DRAG_MS 400

bool sim_headless_step(void)
{
    if (g_done || g_pos >= g_n) {
        return true;
    }
    uint32_t now = lv_tick_get();
    step_t *s = &g_steps[g_pos];
    if (!g_started) {
        g_started = true;
        g_t0 = now;
    }
    uint32_t el = now - g_t0;
    bool finished = false;

    switch (s->op) {
    case OP_WAIT:
        finished = el >= (uint32_t)s->a;
        break;
    case OP_TAP:
        g_px = s->a; g_py = s->b;
        g_pressed = el < TAP_MS;
        finished = el >= TAP_MS + 60; /* danach Zeit zum Verarbeiten des Release */
        break;
    case OP_DRAG: {
        float f = el >= DRAG_MS ? 1.0f : (float)el / DRAG_MS;
        g_px = s->a + (int)((s->c - s->a) * f);
        g_py = s->b + (int)((s->d - s->b) * f);
        g_pressed = el < DRAG_MS;
        finished = el >= DRAG_MS + 60;
        break;
    }
    case OP_SHOT:
        write_ppm(s->name);
        finished = true;
        break;
    case OP_QUIT:
        g_done = true;
        return true;
    }
    if (finished) {
        g_pressed = false;
        g_pos++;
        g_started = false;
    }
    return g_pos >= g_n;
}

void sim_headless_pointer(int x, int y, bool down)
{
    g_px = x;
    g_py = y;
    g_pressed = down;
}

const uint16_t *sim_headless_framebuffer(void)
{
    return g_fb;
}

bool sim_headless_take_dirty(void)
{
    bool d = g_dirty;
    g_dirty = false;
    return d;
}
