/*
 * sheroes_menu.c: the port's own menu and its saved options.
 *
 * Press + and - together to open or close it. While it is open the game sees
 * no input (+ alone pauses it first, which is fine). D-pad or left stick:
 * up/down choose a row, left/right change it; A changes it too; B closes.
 * Closing saves sheroes_options.txt (sdmc:/switch/sheroes/ on the Switch, the
 * working folder elsewhere; SHEROES_OPTIONS=<path> overrides).
 *
 * Settings and where they act:
 *   button layout, per-button map  the USB report filter (usb_gamepad_filter):
 *                                  which pad button each Xbox button reads
 *   invert camera (right stick)    the same filter, the RX / RY axes negated
 *   aspect 4:3 / 16:9              RECOMP_WIDESCREEN, at the next start
 *   hide blurry edges              nv2a_present_crop_rt (lines cropped top and
 *                                  bottom, the picture zoomed to fill). From
 *                                  CoD-nx, whose game smears 16 lines at each
 *                                  edge; Sonic Heroes shows no such bands on
 *                                  the PC, so it defaults to Off here
 *   render resolution, FXAA,       nv2a_vk_scale_pct, nv2a_vk_fxaa,
 *   texture filtering              nv2a_vk_aniso (Vulkan renderer)
 *   FPS counter                    drawn by the overlay
 *   music volume                   mcpx_apu_stream_gain: this game streams its
 *                                  music as stereo PCM voices (v062/v063 in
 *                                  game, bins 0/1 at -18 and -24 dB) and plays
 *                                  effects from mono ADPCM buffers (Linux run,
 *                                  RECOMP_APU_BINS=1, 2026-10-27). Default 200%:
 *                                  the toolkit's DSP is a passthrough that
 *                                  leaves the 2D bins' 6 dB headroom (submix
 *                                  headroom 1) uncompensated while the 3D
 *                                  effects' HRTF bins have none
 *
 * The menu and the counter are drawn on the CPU into small pixel blocks the
 * Vulkan renderer copies into the picture before it is shown
 * (nv2a_overlay_fn). Font: sheroes_font8x8.h (public domain).
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../xboxrecomp/src/platform/win32_clock.h"   /* sheroes-nx: clock_gettime on Windows */

#include "sheroes_menu.h"
#include "sheroes_font8x8.h"
#include "../xboxrecomp/src/kernel/nv2a_backend.h"
#include "../xboxrecomp/src/usb/usb_gamepad.h"

/* xboxrecomp/src/apu/apu_vp.c: gain on stereo streamed voices (the music). */
extern volatile float mcpx_apu_stream_gain;

#ifdef NFSU2_VULKAN
extern volatile int nv2a_vk_fxaa, nv2a_vk_aniso, nv2a_vk_scale_pct;
#endif

/* ---- options ----------------------------------------------------------- */

/* Pad slots in the report, by the Switch button that fills them (xinput_nx.c
 * maps by label): A B X Y R L ZL ZR = Xbox A B X Y Black White LT RT. */
static const char *const s_pad_name[8] = { "A", "B", "X", "Y", "R", "L", "ZL", "ZR" };
static const unsigned char s_map_xbox[8]  = { 1, 0, 3, 2, 4, 5, 6, 7 };  /* Xbox positions */
static const unsigned char s_map_label[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };  /* Switch labels */

enum { LAYOUT_XBOX, LAYOUT_LABEL, LAYOUT_CUSTOM };

typedef struct {
    int layout;
    unsigned char map[8];       /* Xbox slot k reads pad slot map[k] */
    int inv_x, inv_y;
    int widescreen;
    int crop;                   /* index into s_crop_lines */
    int scale;                  /* index into s_scale_pct */
    int fxaa;
    int aniso;                  /* index into s_aniso_val */
    int fps;
    int music;                  /* index into s_music_pct */
} Options;

static const int s_crop_lines[] = { 0, 16, 24 };
static const int s_scale_pct[]  = { 100, 150, 200 };
static const int s_aniso_val[]  = { 1, 4, 16 };
static const int s_music_pct[]  = { 100, 150, 200, 250, 300 };
#define N_OF(a) ((int)(sizeof(a) / sizeof((a)[0])))

static Options s_opt;
static volatile int s_open;

static void opt_defaults(Options *o)
{
    memset(o, 0, sizeof *o);
    o->layout = LAYOUT_XBOX;
    memcpy(o->map, s_map_xbox, 8);
    o->crop = 0;                /* Sonic Heroes has no edge bands (CoD-nx did) */
    o->scale = 0;               /* 1x (Mark, 2026-10-29) */
    o->widescreen = 0;          /* 4:3 (Mark, 2026-10-29) */
    o->fxaa = 1;
    o->aniso = 2;
    o->music = 2;
}

static const char *opt_path(void)
{
    const char *e = getenv("SHEROES_OPTIONS");
    if (e && *e)
        return e;
#ifdef __SWITCH__
    return "sdmc:/switch/sheroes/sheroes_options.txt";
#else
    return "sheroes_options.txt";
#endif
}

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

static int opt_load(Options *o)
{
    FILE *f = fopen(opt_path(), "r");
    char line[128];
    int k;

    if (!f)
        return 0;
    while (fgets(line, sizeof line, f)) {
        char *eq = strchr(line, '=');
        int v;
        if (!eq)
            continue;
        *eq = 0;
        v = atoi(eq + 1);
        if (!strcmp(line, "layout"))          o->layout = clampi(v, 0, 2);
        else if (!strcmp(line, "invert_x"))   o->inv_x = !!v;
        else if (!strcmp(line, "invert_y"))   o->inv_y = !!v;
        else if (!strcmp(line, "widescreen")) o->widescreen = !!v;
        else if (!strcmp(line, "crop"))       o->crop = clampi(v, 0, N_OF(s_crop_lines) - 1);
        else if (!strcmp(line, "scale"))      o->scale = clampi(v, 0, N_OF(s_scale_pct) - 1);
        else if (!strcmp(line, "fxaa"))       o->fxaa = !!v;
        else if (!strcmp(line, "aniso"))      o->aniso = clampi(v, 0, N_OF(s_aniso_val) - 1);
        else if (!strcmp(line, "fps"))        o->fps = !!v;
        else if (!strcmp(line, "music"))      o->music = clampi(v, 0, N_OF(s_music_pct) - 1);
        else if (!strncmp(line, "map", 3) && line[3] >= '0' && line[3] <= '7' && !line[4])
            o->map[line[3] - '0'] = (unsigned char)clampi(v, 0, 7);
    }
    fclose(f);
    if (o->layout == LAYOUT_XBOX)
        memcpy(o->map, s_map_xbox, 8);
    else if (o->layout == LAYOUT_LABEL)
        memcpy(o->map, s_map_label, 8);
    for (k = 0; k < 8; k++)
        o->map[k] &= 7;
    return 1;
}

static void opt_save(const Options *o)
{
    FILE *f = fopen(opt_path(), "w");
    int k;

    if (!f) {
        fprintf(stderr, "[menu] cannot write %s\n", opt_path());
        return;
    }
    fprintf(f, "layout=%d\ninvert_x=%d\ninvert_y=%d\nwidescreen=%d\ncrop=%d\nscale=%d\n"
            "fxaa=%d\naniso=%d\nfps=%d\nmusic=%d\n", o->layout, o->inv_x, o->inv_y, o->widescreen,
            o->crop, o->scale, o->fxaa, o->aniso, o->fps, o->music);
    for (k = 0; k < 8; k++)
        fprintf(f, "map%d=%d\n", k, o->map[k]);
    fclose(f);
    fprintf(stderr, "[menu] options saved\n");
}

/* The settings that act at once. */
static void opt_apply(const Options *o)
{
    nv2a_present_crop_rt = s_crop_lines[o->crop];
    mcpx_apu_stream_gain = (float)s_music_pct[o->music] / 100.0f;
#ifdef NFSU2_VULKAN
    nv2a_vk_scale_pct = s_scale_pct[o->scale];
    nv2a_vk_fxaa = o->fxaa;
    nv2a_vk_aniso = s_aniso_val[o->aniso];
#endif
}

/* ---- rows -------------------------------------------------------------- */

enum {
    R_TITLE_CTRL, R_LAYOUT, R_MAP0, R_MAP7 = R_MAP0 + 7, R_INV_X, R_INV_Y,
    R_TITLE_DISP, R_ASPECT, R_CROP, R_SCALE, R_FXAA, R_ANISO, R_FPS, R_MUSIC,
    R_DEFAULTS, R_CLOSE, R_COUNT
};

static const char *const s_map_label_txt[8] = {
    "  Xbox A (bottom)", "  Xbox B (right)", "  Xbox X (left)", "  Xbox Y (top)",
    "  Xbox Black", "  Xbox White", "  Xbox L trigger", "  Xbox R trigger",
};

static int row_selectable(int r)
{
    return r != R_TITLE_CTRL && r != R_TITLE_DISP;
}

/* Label, value and the note shown on the bottom line when selected. */
static void row_text(int r, const Options *o, const char **label, char *val, size_t vn,
                     const char **note)
{
    static const char *const layouts[] = { "Xbox positions", "Switch labels", "Custom" };
    static const char *const crops[] = { "Off", "16 lines", "24 lines" };
    static const char *const scales[] = { "Native", "1.5x", "2x" };
    static const char *const anisos[] = { "Off", "4x", "16x" };

    *label = "";
    *note = "";
    val[0] = 0;
    if (r >= R_MAP0 && r <= R_MAP7) {
        *label = s_map_label_txt[r - R_MAP0];
        snprintf(val, vn, "%s", s_pad_name[o->map[r - R_MAP0]]);
        *note = "The Switch button for this Xbox one";
        return;
    }
    switch (r) {
    case R_TITLE_CTRL: *label = "CONTROLS"; break;
    case R_LAYOUT:
        *label = "Button layout";
        snprintf(val, vn, "%s", layouts[o->layout]);
        *note = o->layout == LAYOUT_XBOX ? "Buttons where the Xbox had them"
              : o->layout == LAYOUT_LABEL ? "Switch A is Xbox A, and so on" : "Set below";
        break;
    case R_INV_X:
        *label = "Invert camera L/R";
        snprintf(val, vn, "%s", o->inv_x ? "On" : "Off");
        *note = "Right stick, left and right";
        break;
    case R_INV_Y:
        *label = "Invert camera U/D";
        snprintf(val, vn, "%s", o->inv_y ? "On" : "Off");
        *note = "Right stick, up and down";
        break;
    case R_TITLE_DISP: *label = "DISPLAY AND SOUND"; break;
    case R_ASPECT:
        *label = "Aspect ratio";
        snprintf(val, vn, "%s", o->widescreen ? "16:9" : "4:3");
        *note = "Takes effect after a restart";
        break;
    case R_CROP:
        *label = "Hide blurry edges";
        snprintf(val, vn, "%s", crops[o->crop]);
        *note = "Crops the top and bottom edges";
        break;
    case R_SCALE:
        *label = "Resolution";
        snprintf(val, vn, "%s", scales[o->scale]);
        *note = "Higher is sharper but slower";
        break;
    case R_FXAA:
        *label = "Smoothing (FXAA)";
        snprintf(val, vn, "%s", o->fxaa ? "On" : "Off");
        *note = "Softens jagged edges";
        break;
    case R_ANISO:
        *label = "Texture filtering";
        snprintf(val, vn, "%s", anisos[o->aniso]);
        *note = "Sharper floors at an angle";
        break;
    case R_FPS:
        *label = "FPS counter";
        snprintf(val, vn, "%s", o->fps ? "On" : "Off");
        *note = "Top left corner";
        break;
    case R_MUSIC:
        *label = "Music volume";
        snprintf(val, vn, "%d%%", s_music_pct[o->music]);
        *note = "On top of the game's own setting";
        break;
    case R_DEFAULTS:
        *label = "Reset to defaults";
        *note = "Press A";
        break;
    case R_CLOSE:
        *label = "Save and close";
        *note = "Press A (or B, or + and -)";
        break;
    }
}

static int wrap(int v, int d, int n) { return ((v + d) % n + n) % n; }

static void row_change(int r, int d, Options *o)
{
    if (r >= R_MAP0 && r <= R_MAP7) {
        o->map[r - R_MAP0] = (unsigned char)wrap(o->map[r - R_MAP0], d, 8);
        o->layout = !memcmp(o->map, s_map_xbox, 8) ? LAYOUT_XBOX
                  : !memcmp(o->map, s_map_label, 8) ? LAYOUT_LABEL : LAYOUT_CUSTOM;
        return;
    }
    switch (r) {
    case R_LAYOUT:
        o->layout = wrap(o->layout == LAYOUT_CUSTOM ? LAYOUT_LABEL : o->layout, d, 2);
        memcpy(o->map, o->layout == LAYOUT_XBOX ? s_map_xbox : s_map_label, 8);
        break;
    case R_INV_X:  o->inv_x ^= 1; break;
    case R_INV_Y:  o->inv_y ^= 1; break;
    case R_ASPECT: o->widescreen ^= 1; break;
    case R_CROP:   o->crop = wrap(o->crop, d, N_OF(s_crop_lines)); break;
    case R_SCALE:  o->scale = wrap(o->scale, d, N_OF(s_scale_pct)); break;
    case R_FXAA:   o->fxaa ^= 1; break;
    case R_ANISO:  o->aniso = wrap(o->aniso, d, N_OF(s_aniso_val)); break;
    case R_FPS:    o->fps ^= 1; break;
    case R_MUSIC:  o->music = wrap(o->music, d, N_OF(s_music_pct)); break;
    }
    opt_apply(o);
}

/* ---- input ------------------------------------------------------------- */

static volatile int s_sel = R_LAYOUT;

static uint64_t now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
}

static int16_t rd16(const uint8_t *p) { return (int16_t)(p[0] | (p[1] << 8)); }
static void wr16(uint8_t *p, int v)
{
    if (v > 32767) v = 32767;
    if (v < -32768) v = -32768;
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)(((unsigned)v >> 8) & 0xFF);
}

static void menu_close(void)
{
    s_open = 0;
    opt_save(&s_opt);
}

static void menu_select(int d)
{
    int r = s_sel;
    do
        r = wrap(r, d, R_COUNT);
    while (!row_selectable(r));
    s_sel = r;
}

/* One step of the menu from pad 1's raw report. */
static void menu_input(const uint8_t *rep)
{
    enum { K_UP = 1, K_DOWN = 2, K_LEFT = 4, K_RIGHT = 8, K_A = 16, K_B = 32 };
    static unsigned held;
    static uint64_t next_repeat;
    int16_t lx = rd16(rep + 12), ly = rd16(rep + 14);
    unsigned k = 0, edge;
    uint64_t t = now_ms();

    if ((rep[2] & 0x01) || ly > 16000)  k |= K_UP;
    if ((rep[2] & 0x02) || ly < -16000) k |= K_DOWN;
    if ((rep[2] & 0x04) || lx < -16000) k |= K_LEFT;
    if ((rep[2] & 0x08) || lx > 16000)  k |= K_RIGHT;
    if (rep[4] > 0x40) k |= K_A;
    if (rep[5] > 0x40) k |= K_B;
    edge = k & ~held;
    if (k & (K_UP | K_DOWN | K_LEFT | K_RIGHT)) {
        if (edge & (K_UP | K_DOWN | K_LEFT | K_RIGHT))
            next_repeat = t + 350;
        else if (t >= next_repeat) {
            edge |= k & (K_UP | K_DOWN | K_LEFT | K_RIGHT);
            next_repeat = t + 110;
        }
    }
    held = k;
    if (edge && getenv("SHEROES_MENU_TRACE"))
        fprintf(stderr, "[menu] t=%u keys %02X edge %02X\n", (unsigned)(t % 100000), k, edge);

    if (edge & K_UP)    menu_select(-1);
    if (edge & K_DOWN)  menu_select(1);
    if (edge & K_LEFT)  row_change(s_sel, -1, &s_opt);
    if (edge & K_RIGHT) row_change(s_sel, 1, &s_opt);
    if (edge & K_A) {
        if (s_sel == R_CLOSE)
            menu_close();
        else if (s_sel == R_DEFAULTS) {
            int ws = s_opt.widescreen;
            opt_defaults(&s_opt);
            s_opt.widescreen = ws;  /* needs a restart; left as chosen */
            opt_apply(&s_opt);
        } else
            row_change(s_sel, 1, &s_opt);
    }
    if (edge & K_B)
        menu_close();
}

static void pad_filter(int dev, uint8_t *rep)
{
    static int combo_held, quiet;
    uint8_t in[8];
    int k, combo;

    if (dev == 0) {
        static int trace = -1;
        if (trace < 0)
            trace = getenv("SHEROES_MENU_TRACE") != NULL;
        if (trace) {
            static uint64_t t0;
            static unsigned calls;
            uint64_t t = now_ms();
            calls++;
            if (t - t0 >= 2000) {
                fprintf(stderr, "[menu] %u reports in %u ms, digital %02X\n", calls,
                        (unsigned)(t - t0), rep[2]);
                calls = 0;
                t0 = t;
            }
        }
        combo = (rep[2] & 0x30) == 0x30;
        if (combo && !combo_held) {
            if (s_open)
                menu_close();
            else {
                s_open = 1;
                fprintf(stderr, "[menu] open\n");
            }
            quiet = 1;
        } else if (s_open && !combo) {
            menu_input(rep);
            if (!s_open)
                quiet = 1;
        }
        combo_held = combo;
        /* After the menu: nothing until every button is let go, so the
         * press that closed it does not reach the game. */
        if (quiet && !s_open) {
            int any = rep[2] != 0 || rep[3] != 0;
            for (k = 0; k < 8; k++)
                any |= rep[4 + k] > 0x40;
            if (!any)
                quiet = 0;
        }
        if (s_open || quiet) {
            memset(rep + 2, 0, 18);
            return;
        }
    }
    memcpy(in, rep + 4, 8);
    for (k = 0; k < 8; k++)
        rep[4 + k] = in[s_opt.map[k] & 7];
    if (s_opt.inv_x)
        wr16(rep + 16, -(int)rd16(rep + 16));
    if (s_opt.inv_y)
        wr16(rep + 18, -(int)rd16(rep + 18));
}

/* ---- drawing ----------------------------------------------------------- */

#define COLS 38
#define ROWS (R_COUNT + 2)               /* title, the rows, the note */
#define LINE 9                           /* font rows per line */
#define C_BG     0xF0100C10u
#define C_BORDER 0xFF8C1C1Cu
#define C_SEL    0xFF3C1616u
#define C_TEXT   0xFFC8C8C8u
#define C_HEAD   0xFFD04040u
#define C_TITLE  0xFFE8D8A0u
#define C_VALUE  0xFFE8D8A0u
#define C_NOTE   0xFF909090u

typedef struct {
    uint32_t *px;
    int w, h, fx, fy;
} Canvas;

static void fill(Canvas *c, int x, int y, int w, int h, uint32_t col)
{
    int i, j;
    for (j = y < 0 ? 0 : y; j < y + h && j < c->h; j++)
        for (i = x < 0 ? 0 : x; i < x + w && i < c->w; i++)
            c->px[j * c->w + i] = col;
}

static void text(Canvas *c, int x, int y, const char *s, uint32_t col)
{
    for (; *s; s++, x += 8 * c->fx) {
        const unsigned char *g = sheroes_font8x8[(unsigned char)*s & 0x7F];
        int r, b;
        for (r = 0; r < 8; r++)
            for (b = 0; b < 8; b++)
                if (g[r] & (1u << b))
                    fill(c, x + b * c->fx, y + r * c->fy, c->fx, c->fy, col);
    }
}

/* Font pixel size: 8x8 glyphs drawn at 16x16 of a 640x480 picture. */
static void font_scale(int pw, int ph, int lw, int lh, int *fx, int *fy)
{
    *fx = lw > 0 ? (int)(2.0 * pw / lw + 0.5) : 2;
    *fy = lh > 0 ? (int)(2.0 * ph / lh + 0.5) : 2;
    if (*fx < 1) *fx = 1;
    if (*fy < 1) *fy = 1;
    while (*fx > 1 && COLS * 8 * *fx + 4 * *fx > pw) (*fx)--;
    while (*fy > 1 && ROWS * LINE * *fy + 4 * *fy > ph - 2 * nv2a_present_crop_y() * ph / (lh > 0 ? lh : 480)) (*fy)--;
}

static void draw_menu(Canvas *c)
{
    const Options *o = &s_opt;
    int lh = LINE * c->fy, pad = 2 * c->fx, x0 = pad + 4 * c->fx, y, r, sel = s_sel;
    int right = c->w - pad - 4 * c->fx;
    const char *note = "";

    fill(c, 0, 0, c->w, c->h, C_BG);
    fill(c, 0, 0, c->w, c->fy, C_BORDER);
    fill(c, 0, c->h - c->fy, c->w, c->fy, C_BORDER);
    fill(c, 0, 0, c->fx, c->h, C_BORDER);
    fill(c, c->w - c->fx, 0, c->fx, c->h, C_BORDER);

    y = pad + c->fy;
    text(c, x0, y, "CURSE OF DARKNESS: PORT MENU", C_TITLE);
    y += lh;
    for (r = 0; r < R_COUNT; r++, y += lh) {
        const char *label, *rn;
        char val[32];
        int head = !row_selectable(r);
        row_text(r, o, &label, val, sizeof val, &rn);
        if (r == sel) {
            fill(c, pad, y - (c->fy + 1) / 2, c->w - 2 * pad, lh, C_SEL);
            note = rn;
        }
        text(c, x0, y, label, head ? C_HEAD : r == sel ? 0xFFFFFFFFu : C_TEXT);
        if (val[0]) {
            char b[40];
            if (r == sel)
                snprintf(b, sizeof b, "< %s >", val);
            else
                snprintf(b, sizeof b, "%s", val);
            text(c, right - (int)strlen(b) * 8 * c->fx, y, b, C_VALUE);
        }
    }
    text(c, x0, y, note, C_NOTE);
}

static volatile int s_fps_shown;

static void draw_fps(Canvas *c)
{
    char b[16];
    fill(c, 0, 0, c->w, c->h, 0xC0000000u);
    snprintf(b, sizeof b, "%3d FPS", s_fps_shown);
    text(c, c->fx, c->fy, b, 0xFF60FF60u);
}

enum { OV_MENU = 1, OV_FPS = 2 };

static int overlay(int i, int pw, int ph, int lw, int lh, int *x, int *y, int *w, int *h,
                   uint32_t *px)
{
    static int kinds[2], n;
    int fx, fy, crop, kind;
    Canvas c;

    if (i == 0 && !px) {
        static uint64_t t0;
        static int frames;
        uint64_t t = now_ms();
        frames++;
        if (!t0)
            t0 = t;
        if (t - t0 >= 1000) {
            s_fps_shown = (int)(frames * 1000 / (t - t0));
            frames = 0;
            t0 = t;
        }
        n = 0;
        if (s_open)
            kinds[n++] = OV_MENU;
        if (s_opt.fps)
            kinds[n++] = OV_FPS;
    }
    if (i >= n)
        return 0;
    kind = kinds[i];
    font_scale(pw, ph, lw, lh, &fx, &fy);
    if (kind == OV_MENU) {
        *w = COLS * 8 * fx + 4 * fx;
        *h = ROWS * LINE * fy + 4 * fy;
        *x = (pw - *w) / 2;
        *y = (ph - *h) / 2;
    } else {
        fx = fx > 1 ? fx / 2 : 1;          /* the counter at half size */
        fy = fy > 1 ? fy / 2 : 1;
        crop = lh > 0 ? nv2a_present_crop_y() * ph / lh : 0;
        *w = 9 * 8 * fx;
        *h = 10 * fy;
        *x = 8 * fx;
        *y = crop + 4 * fy;
    }
    if (!px)
        return 1;
    c.px = px;
    c.w = *w;
    c.h = *h;
    c.fx = fx;
    c.fy = fy;
    if (kind == OV_MENU)
        draw_menu(&c);
    else
        draw_fps(&c);
    return 1;
}

/* ---- boot -------------------------------------------------------------- */

void sheroes_menu_init(void)
{
    opt_defaults(&s_opt);
    if (opt_load(&s_opt))
        fprintf(stderr, "[menu] options loaded from %s\n", opt_path());
    /* The game reads the TV setting once, at boot. */
#ifdef _WIN32
    _putenv(s_opt.widescreen ? "RECOMP_WIDESCREEN=1" : "RECOMP_WIDESCREEN=0");
#else
    setenv("RECOMP_WIDESCREEN", s_opt.widescreen ? "1" : "0", 1);
#endif
    opt_apply(&s_opt);
    usb_gamepad_filter = pad_filter;
    nv2a_overlay_fn = overlay;
}
