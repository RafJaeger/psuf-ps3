#include "ui.h"

#include <malloc.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#ifndef FPSU_UI_HOST
#include <rsx/gcm_sys.h>
#include <rsx/rsx.h>
#include <sysutil/msg.h>
#include <sysutil/sysutil.h>
#include <sysutil/video.h>
#endif

#define UI_CB_SIZE 0x100000
#define UI_HOST_SIZE (8 * 1024 * 1024)
#define UI_LOGICAL_WIDTH 1280
#define UI_LOGICAL_HEIGHT 720

#ifndef FPSU_UI_HOST
static gcmContextData *g_context;
#endif
static void *g_host;
static u32 *g_color_buffers[2];
static u32 *g_canvas;
static int g_dark_theme;
static int g_theme_mode;
static u32 g_color_offsets[2];
static u32 g_color_pitch;
static int g_draw_buffer;
static int g_width = 1280;
static int g_height = 720;
static int g_view_x;
static int g_view_y;
static int g_view_w = UI_LOGICAL_WIDTH;
static int g_view_h = UI_LOGICAL_HEIGHT;
static volatile int g_dialog_result;
static volatile int g_exit_requested;
static int g_ready;
static int g_live_open;
static void *g_brand_pixels;
static unsigned int g_brand_pitch;
static unsigned int g_brand_width;
static unsigned int g_brand_height;
typedef struct {
    unsigned char *data;
    unsigned int width, height, count, frame, delays[64];
    u64 next_frame;
} ui_animation;
static ui_animation g_animations[2];
typedef struct { int x, y, w, h, home; } ui_brand_slot;
static ui_brand_slot g_brand_slots[4];
static int g_brand_slot_count;
static int g_record_brand_slots;
static int g_flip_pending;
static int g_system_menu_open;
static int g_system_drawing;
#ifndef FPSU_UI_HOST
static int g_last_flip_buffer = -1;
#endif
static int g_frame_writable = 1;

int ui_is_dark(void) { return g_theme_mode == UI_THEME_DARK; }

int ui_theme_get(void) { return g_theme_mode; }

int ui_set_theme(int theme)
{
    char saved;
    if (theme < UI_THEME_NORMAL || theme > UI_THEME_DARK) return -1;
    saved = (char)('0' + theme);
#if !defined(FPSU_UI_HOST) || defined(FPSU_UI_TEST_PERSIST)
    FILE *out;
    int written, flushed, closed;
    mkdir(FPSU_DATA_ROOT, 0777);
    out = fopen(FPSU_DATA_ROOT "/theme.tmp", "wb");
    if (!out) return -1;
    written = fputc(saved, out);
    flushed = fflush(out);
    if (flushed == 0) flushed = fsync(fileno(out));
    closed = fclose(out);
    if (written == EOF || flushed != 0 || closed != 0) {
        remove(FPSU_DATA_ROOT "/theme.tmp");
        return -1;
    }
    if (rename(FPSU_DATA_ROOT "/theme.tmp", FPSU_DATA_ROOT "/theme.txt") != 0) {
        /* GameOS can reject replacing an existing destination. Keep the old
           preference recoverable until the new file has been published. */
        remove(FPSU_DATA_ROOT "/theme.previous");
        if (rename(FPSU_DATA_ROOT "/theme.txt", FPSU_DATA_ROOT "/theme.previous") != 0)
            return -1;
        if (rename(FPSU_DATA_ROOT "/theme.tmp", FPSU_DATA_ROOT "/theme.txt") != 0) {
            rename(FPSU_DATA_ROOT "/theme.previous", FPSU_DATA_ROOT "/theme.txt");
            return -1;
        }
    }
    remove(FPSU_DATA_ROOT "/theme.previous");
#endif
    g_theme_mode = theme;
    g_dark_theme = theme == UI_THEME_DARK;
    return 0;
}

int ui_set_dark(int dark)
{
    return ui_set_theme(dark ? UI_THEME_DARK : UI_THEME_NORMAL);
}

static void ui_load_theme(void)
{
#if !defined(FPSU_UI_HOST) || defined(FPSU_UI_TEST_PERSIST)
    FILE *theme = fopen(FPSU_DATA_ROOT "/theme.txt", "rb");
    if (!theme) theme = fopen(FPSU_DATA_ROOT "/theme.previous", "rb");
    if (theme) {
        int saved = fgetc(theme);
        if (saved == '0' || saved == '1') {
            g_theme_mode = saved - '0';
            g_dark_theme = g_theme_mode == UI_THEME_DARK;
        }
        fclose(theme);
    }
#endif
}

static u32 surface_color(u32 color)
{
    if (!g_dark_theme) return color;
    switch (color) {
    case UI_COLOR_BG_TOP: return 0xff000000u;
    case UI_COLOR_BG_BOTTOM: return 0xff080809u;
    case 0xff08141eu: return 0xff000000u;
    case 0xff102631u: case 0xff122c38u: case 0xff102834u: return 0xff1c1c1eu;
    case 0xff0a1d28u: case 0xff0d2330u: case 0xff0c202cu: return 0xff101012u;
    case 0xff163241u: case 0xff16303cu: case UI_COLOR_PANEL:
    case 0xff203244u: case 0xff102632u:
    case UI_COLOR_PANEL_SOFT: return 0xff242426u;
    case 0xff214b5au: case 0xff204653u: case UI_COLOR_PANEL_INNER: return 0xff133c40u;
    case 0xff233e49u: case 0xff234956u: case 0xff203a45u: case 0xff294551u:
    case 0xff28444fu: case UI_COLOR_PANEL_EDGE: return 0xff3a3a3cu;
    default: return color;
    }
}

static u32 text_color(u32 color)
{
    if (!g_dark_theme) return color;
    switch (color) {
    case UI_COLOR_TEXT: return 0xfff5f5f7u;
    case UI_COLOR_MUTED: return 0xffc8c8cdu;
    case UI_COLOR_DISABLED: return 0xff92929au;
    default: return color;
    }
}

static u32 *frame_pixels(void)
{
    return g_canvas ? g_canvas : g_frame_writable ? g_color_buffers[g_draw_buffer] : NULL;
}
typedef struct { const unsigned char *mask; unsigned char w, h, advance; } ui_glyph;
static unsigned char *g_font_data;
static ui_glyph g_glyphs[4][224];

static unsigned int read_le32_ui(const unsigned char *p)
{
    return p[0] | ((unsigned int)p[1] << 8) | ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24);
}

static unsigned char *load_ui_asset(const char *name, size_t limit, size_t *size)
{
    char path[1024]; FILE *in; long len; unsigned char *data;
    snprintf(path, sizeof(path), "%s/%s", FPSU_APP_USRDIR, name);
    in = fopen(path, "rb");
    if (!in) return NULL;
    if (fseek(in, 0, SEEK_END) || (len = ftell(in)) <= 0 || (size_t)len > limit || fseek(in, 0, SEEK_SET)) {
        fclose(in); return NULL;
    }
    data = malloc((size_t)len);
    if (!data || fread(data, 1, (size_t)len, in) != (size_t)len) { free(data); fclose(in); return NULL; }
    fclose(in); *size = (size_t)len; return data;
}

static void load_ui_animation(const char *name, ui_animation *animation)
{
    size_t size = 0;
    unsigned char *data = load_ui_asset(name, 8 * 1024 * 1024, &size);
    if (data && size >= 16 && memcmp(data, "UIA1", 4) == 0) {
        unsigned int w = read_le32_ui(data + 4), h = read_le32_ui(data + 8);
        unsigned int frames = read_le32_ui(data + 12);
        size_t header = 16 + (size_t)frames * 4;
        if (w && h && w <= 512 && h <= 256 && frames && frames <= 64 &&
            size == header + (size_t)w * h * frames * 4) {
            size_t i;
            for (i = 0; i < frames; ++i) {
                unsigned int delay = read_le32_ui(data + 16 + i * 4);
                animation->delays[i] = delay < 30 ? 30 : delay > 10000 ? 10000 : delay;
            }
            for (i = header; i < size; i += 4) {
                u32 pixel = 0xff000000u | ((u32)data[i + 1] << 16) |
                    ((u32)data[i + 2] << 8) | data[i + 3];
                memcpy(data + i, &pixel, sizeof(pixel));
            }
            animation->data = data;
            animation->count = frames;
            animation->width = w;
            animation->height = h;
            return;
        }
    }
    free(data);
}

static void load_ui_assets(void)
{
    unsigned char *data; size_t size = 0, offset; int f, c;
    data = load_ui_asset("brand.argb", 512 * 1024, &size);
    if (data && size >= 12 && memcmp(data, "UIB1", 4) == 0) {
        unsigned int w = read_le32_ui(data + 4), h = read_le32_ui(data + 8);
        if (w && h && w <= 512 && h <= 256 && size == 12 + (size_t)w * h * 4) {
            u32 *pixels = malloc((size_t)w * h * 4); size_t i;
            if (pixels) {
                for (i = 0; i < (size_t)w * h; ++i) {
                    unsigned char *p = data + 12 + i * 4;
                    pixels[i] = 0xff000000u | ((u32)p[1] << 16) | ((u32)p[2] << 8) | p[3];
                }
                g_brand_pixels = pixels; g_brand_width = w; g_brand_height = h; g_brand_pitch = w * 4;
            }
        }
    }
    free(data);
    load_ui_animation("brand.anim", &g_animations[0]);
    load_ui_animation("home.anim", &g_animations[1]);
    data = load_ui_asset("font.uif", 2 * 1024 * 1024, &size);
    if (!data || size < 8 || memcmp(data, "UIF1", 4) || read_le32_ui(data + 4) != 4) { free(data); return; }
    offset = 8;
    for (f = 0; f < 4; ++f) for (c = 0; c < 224; ++c) {
        ui_glyph *g = &g_glyphs[f][c]; size_t bytes;
        if (offset + 4 > size) goto invalid_font;
        g->w = data[offset]; g->h = data[offset + 1]; g->advance = data[offset + 2]; offset += 4;
        bytes = (size_t)g->w * g->h;
        if (!g->w || g->w > 64 || !g->h || g->h > 64 || g->advance > 64 || offset + bytes > size) goto invalid_font;
        g->mask = data + offset; offset += bytes;
    }
    g_font_data = data; return;
invalid_font:
    memset(g_glyphs, 0, sizeof(g_glyphs)); free(data);
}

static const u8 g_font_digits[10][7] = {
    {0x0e, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0e},
    {0x04, 0x0c, 0x04, 0x04, 0x04, 0x04, 0x0e},
    {0x0e, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1f},
    {0x1e, 0x01, 0x01, 0x0e, 0x01, 0x01, 0x1e},
    {0x02, 0x06, 0x0a, 0x12, 0x1f, 0x02, 0x02},
    {0x1f, 0x10, 0x1e, 0x01, 0x01, 0x11, 0x0e},
    {0x06, 0x08, 0x10, 0x1e, 0x11, 0x11, 0x0e},
    {0x1f, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08},
    {0x0e, 0x11, 0x11, 0x0e, 0x11, 0x11, 0x0e},
    {0x0e, 0x11, 0x11, 0x0f, 0x01, 0x02, 0x0c}
};

static const u8 g_font_alpha[26][7] = {
    {0x0e, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11},
    {0x1e, 0x11, 0x11, 0x1e, 0x11, 0x11, 0x1e},
    {0x0e, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0e},
    {0x1e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1e},
    {0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x1f},
    {0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x10},
    {0x0e, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0f},
    {0x11, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11},
    {0x0e, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0e},
    {0x01, 0x01, 0x01, 0x01, 0x11, 0x11, 0x0e},
    {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11},
    {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1f},
    {0x11, 0x1b, 0x15, 0x15, 0x11, 0x11, 0x11},
    {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11},
    {0x0e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e},
    {0x1e, 0x11, 0x11, 0x1e, 0x10, 0x10, 0x10},
    {0x0e, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0d},
    {0x1e, 0x11, 0x11, 0x1e, 0x14, 0x12, 0x11},
    {0x0f, 0x10, 0x10, 0x0e, 0x01, 0x01, 0x1e},
    {0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04},
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e},
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x0a, 0x04},
    {0x11, 0x11, 0x11, 0x15, 0x15, 0x1b, 0x11},
    {0x11, 0x11, 0x0a, 0x04, 0x0a, 0x11, 0x11},
    {0x11, 0x11, 0x0a, 0x04, 0x04, 0x04, 0x04},
    {0x1f, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1f}
};

static const u8 g_blank[7] = {0, 0, 0, 0, 0, 0, 0};
static const u8 g_dot[7] = {0, 0, 0, 0, 0, 0x0c, 0x0c};
static const u8 g_colon[7] = {0, 0x0c, 0x0c, 0, 0x0c, 0x0c, 0};
static const u8 g_dash[7] = {0, 0, 0, 0x1f, 0, 0, 0};
static const u8 g_plus[7] = {0, 0x04, 0x04, 0x1f, 0x04, 0x04, 0};
static const u8 g_slash[7] = {0x01, 0x02, 0x02, 0x04, 0x08, 0x08, 0x10};
static const u8 g_backslash[7] = {0x10, 0x08, 0x08, 0x04, 0x02, 0x02, 0x01};
static const u8 g_lparen[7] = {0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02};
static const u8 g_rparen[7] = {0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08};
static const u8 g_lbracket[7] = {0x0e, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0e};
static const u8 g_rbracket[7] = {0x0e, 0x02, 0x02, 0x02, 0x02, 0x02, 0x0e};
static const u8 g_bang[7] = {0x04, 0x04, 0x04, 0x04, 0x04, 0, 0x04};
static const u8 g_question[7] = {0x0e, 0x11, 0x01, 0x02, 0x04, 0, 0x04};
static const u8 g_comma[7] = {0, 0, 0, 0, 0, 0x04, 0x08};
static const u8 g_quote[7] = {0x0a, 0x0a, 0x0a, 0, 0, 0, 0};
static const u8 g_percent[7] = {0x19, 0x19, 0x02, 0x04, 0x08, 0x13, 0x13};
static const u8 g_star[7] = {0, 0x15, 0x0e, 0x1f, 0x0e, 0x15, 0};
static const u8 g_hash[7] = {0x0a, 0x0a, 0x1f, 0x0a, 0x1f, 0x0a, 0x0a};
static const u8 g_pipe[7] = {0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04};

static const u8 *glyph_for(char c)
{
    if (c >= 'a' && c <= 'z') {
        c = (char)(c - 32);
    }
    if (c >= 'A' && c <= 'Z') {
        return g_font_alpha[c - 'A'];
    }
    if (c >= '0' && c <= '9') {
        return g_font_digits[c - '0'];
    }

    switch (c) {
    case ' ':
        return g_blank;
    case '.':
        return g_dot;
    case ',':
        return g_comma;
    case ':':
    case ';':
        return g_colon;
    case '-':
    case '_':
        return g_dash;
    case '+':
        return g_plus;
    case '/':
        return g_slash;
    case '\\':
        return g_backslash;
    case '(':
        return g_lparen;
    case ')':
        return g_rparen;
    case '[':
    case '<':
        return g_lbracket;
    case ']':
    case '>':
        return g_rbracket;
    case '!':
        return g_bang;
    case '?':
        return g_question;
    case '\'':
    case '"':
        return g_quote;
    case '%':
        return g_percent;
    case '*':
        return g_star;
    case '#':
        return g_hash;
    case '|':
        return g_pipe;
    default:
        return g_question;
    }
}

static u8 red_of(u32 color) { return (u8)((color >> 16) & 0xff); }
static u8 green_of(u32 color) { return (u8)((color >> 8) & 0xff); }
static u8 blue_of(u32 color) { return (u8)(color & 0xff); }

static u32 rgb(u8 r, u8 g, u8 b)
{
    return 0xff000000u | ((u32)r << 16) | ((u32)g << 8) | b;
}

static u32 blend_rgb(u32 a, u32 b, int n, int d)
{
    int ar = red_of(a), ag = green_of(a), ab = blue_of(a);
    int br = red_of(b), bg = green_of(b), bb = blue_of(b);
    if (d <= 0) {
        d = 1;
    }
    return rgb((u8)(ar + ((br - ar) * n) / d),
        (u8)(ag + ((bg - ag) * n) / d),
        (u8)(ab + ((bb - ab) * n) / d));
}

static int div_floor_i64(long long n, int d)
{
    if (d <= 0) {
        return 0;
    }
    if (n >= 0) {
        return (int)(n / d);
    }
    return -(int)((-n + d - 1) / d);
}

static int div_ceil_i64(long long n, int d)
{
    if (d <= 0) {
        return 0;
    }
    if (n >= 0) {
        return (int)((n + d - 1) / d);
    }
    return -(int)((-n) / d);
}

static int logical_to_physical_x_floor(int x)
{
    return g_view_x + div_floor_i64((long long)x * g_view_w, UI_LOGICAL_WIDTH);
}

static int logical_to_physical_y_floor(int y)
{
    return g_view_y + div_floor_i64((long long)y * g_view_h, UI_LOGICAL_HEIGHT);
}

static int logical_to_physical_x_ceil(int x)
{
    return g_view_x + div_ceil_i64((long long)x * g_view_w, UI_LOGICAL_WIDTH);
}

static int logical_to_physical_y_ceil(int y)
{
    return g_view_y + div_ceil_i64((long long)y * g_view_h, UI_LOGICAL_HEIGHT);
}

static int logical_to_physical_x_nearest(int x)
{
    return g_view_x + (int)(((long long)x * g_view_w + UI_LOGICAL_WIDTH / 2) / UI_LOGICAL_WIDTH);
}

static int logical_to_physical_y_nearest(int y)
{
    return g_view_y + (int)(((long long)y * g_view_h + UI_LOGICAL_HEIGHT / 2) / UI_LOGICAL_HEIGHT);
}

static int scaled_text_cell_x(int scale)
{
    int cell = (scale * g_view_w + UI_LOGICAL_WIDTH / 2) / UI_LOGICAL_WIDTH;
    if (g_width <= 720 || g_height <= 576) {
        cell = scale;
    }
    return cell > 0 ? cell : 1;
}

static int scaled_text_cell_y(int scale)
{
    int cell = (scale * g_view_h + UI_LOGICAL_HEIGHT / 2) / UI_LOGICAL_HEIGHT;
    if (g_width <= 720 || g_height <= 576) {
        cell = scale;
    }
    return cell > 0 ? cell : 1;
}

static void update_logical_viewport(void)
{
    g_view_x = 0;
    g_view_y = 0;
    g_view_w = g_width;
    g_view_h = g_height;

    if (g_width <= UI_LOGICAL_WIDTH && g_height <= UI_LOGICAL_HEIGHT) {
        int sd_mode = g_width <= 720 || g_height <= 576;
        int margin_x = g_width / (sd_mode ? 20 : 40);
        int margin_y = g_height / (sd_mode ? 20 : 40);
        if (margin_x < 8) {
            margin_x = 8;
        }
        if (margin_y < 8) {
            margin_y = 8;
        }
        if (g_width - margin_x * 2 >= 320 && g_height - margin_y * 2 >= 240) {
            g_view_x = margin_x;
            g_view_y = margin_y;
            g_view_w = g_width - margin_x * 2;
            g_view_h = g_height - margin_y * 2;
        }
    }
}

static void put_pixel(int x, int y, u32 color)
{
    u32 *target = frame_pixels();
    if (!target || x < 0 || y < 0 || x >= g_width || y >= g_height) {
        return;
    }
    target[(y * g_color_pitch / sizeof(u32)) + x] = color;
}

static void raw_fill_rect(int x, int y, int w, int h, unsigned int color)
{
    int xx, yy;
    u32 *target = frame_pixels();
    if (!target || w <= 0 || h <= 0) {
        return;
    }
    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if (x + w > g_width) {
        w = g_width - x;
    }
    if (y + h > g_height) {
        h = g_height - y;
    }
    if (w <= 0 || h <= 0) {
        return;
    }
    for (yy = y; yy < y + h; ++yy) {
        u32 *row = target + (yy * g_color_pitch / sizeof(u32));
        for (xx = x; xx + 7 < x + w; xx += 8) {
            row[xx] = row[xx + 1] = row[xx + 2] = row[xx + 3] = color;
            row[xx + 4] = row[xx + 5] = row[xx + 6] = row[xx + 7] = color;
        }
        for (; xx < x + w; ++xx) {
            row[xx] = color;
        }
    }
}

#ifndef FPSU_UI_HOST
static void ui_boot_trace(const char *phase)
{
    FILE *log = fopen(FPSU_CACHE_ROOT "/startup.log", "a");
    if (log) { fprintf(log, FPSU_VERSION_LABEL " %s\n", phase); fclose(log); }
}
static void dialog_handler(msgButton button, void *usrData)
{
    (void)usrData;
    if (button == MSG_DIALOG_BTN_OK || button == MSG_DIALOG_BTN_YES) {
        g_dialog_result = 1;
    } else if (button == MSG_DIALOG_BTN_NO || button == MSG_DIALOG_BTN_ESCAPE) {
        g_dialog_result = 2;
    } else if (button == MSG_DIALOG_BTN_NONE) {
        g_dialog_result = -1;
    }
}

static void ui_system_callback(u64 status, u64 param, void *usrdata)
{
    (void)param;
    (void)usrdata;
    if (status == SYSUTIL_EXIT_GAME) {
        g_exit_requested = 1;
        g_dialog_result = 2;
    } else if (status == SYSUTIL_MENU_OPEN) {
        g_system_menu_open = 1;
        ui_boot_trace("system_menu_open");
    } else if (status == SYSUTIL_MENU_CLOSE) {
        g_system_menu_open = 0;
        ui_boot_trace("system_menu_close");
    } else if (status == SYSUTIL_DRAW_BEGIN) {
        g_system_drawing = 1;
    } else if (status == SYSUTIL_DRAW_END) {
        g_system_drawing = 0;
    }
}

int ui_init(void)
{
    videoState state;
    videoResolution res;
    videoConfiguration config;

    ui_boot_trace("video_init");
    g_host = memalign(1024 * 1024, UI_HOST_SIZE);
    if (!g_host) {
        return -1;
    }

    if (rsxInit(&g_context, UI_CB_SIZE, UI_HOST_SIZE, g_host) != 0) {
        return -1;
    }

    videoGetState(0, 0, &state);
    videoGetResolution(state.displayMode.resolution, &res);
    memset(&config, 0, sizeof(config));
    config.resolution = state.displayMode.resolution;
    config.format = VIDEO_BUFFER_FORMAT_XRGB;
    config.pitch = res.width * sizeof(u32);
    videoConfigure(0, &config, NULL, 0);

    g_width = res.width;
    g_height = res.height;
    update_logical_viewport();
    g_color_pitch = res.width * sizeof(u32);
    g_canvas = memalign(128, (size_t)g_height * g_color_pitch);
    ui_boot_trace(g_canvas ? "canvas_ram" : "canvas_fallback");
    for (int i = 0; i < 2; ++i) {
        g_color_buffers[i] = (u32 *)rsxMemalign(64, res.height * g_color_pitch);
        if (!g_color_buffers[i]) {
            return -1;
        }
        rsxAddressToOffset(g_color_buffers[i], &g_color_offsets[i]);
        gcmSetDisplayBuffer(i, g_color_offsets[i], g_color_pitch, res.width, res.height);
    }
    g_draw_buffer = 0;
    gcmSetFlipMode(GCM_FLIP_VSYNC);

    gcmResetFlipStatus();
    ui_boot_trace("assets_start");
    load_ui_assets();
    ui_load_theme();
    ui_boot_trace("assets_ready");

    sysUtilRegisterCallback(SYSUTIL_EVENT_SLOT0, ui_system_callback, NULL);
    g_ready = 1;
    ui_begin_frame();
    ui_present();
    ui_boot_trace("first_frame_submitted");
    return 0;
}

void ui_shutdown(void)
{
    if (g_ready) {
        sysUtilUnregisterCallback(SYSUTIL_EVENT_SLOT0);
        if (!g_exit_requested && !ui_system_menu_open()) rsxFinish(g_context, 1);
    }
    if (g_host) {
        free(g_host);
        g_host = NULL;
    }
    if (g_brand_pixels) {
        free(g_brand_pixels);
        g_brand_pixels = NULL;
    }
    free(g_font_data); g_font_data = NULL;
    free(g_animations[0].data); g_animations[0].data = NULL;
    free(g_animations[1].data); g_animations[1].data = NULL;
    free(g_canvas); g_canvas = NULL;
    g_ready = 0;
}

static void wait_dialog(void)
{
    while (g_dialog_result == 0) {
        ui_pump();
        usleep(16000);
    }
    msgDialogAbort();
}

void ui_message(fpsu_lang lang, const char *message)
{
    msgType type = (msgType)(MSG_DIALOG_NORMAL | MSG_DIALOG_BTN_TYPE_OK);
    (void)lang;
    ui_close_live();
    g_dialog_result = 0;
    msgDialogOpen2(type, message, dialog_handler, NULL, NULL);
    wait_dialog();
}

int ui_confirm(fpsu_lang lang, const char *message)
{
    msgType type = (msgType)(MSG_DIALOG_NORMAL | MSG_DIALOG_BTN_TYPE_YESNO |
        MSG_DIALOG_DEFAULT_CURSOR_NO);
    (void)lang;
    ui_close_live();
    g_dialog_result = 0;
    msgDialogOpen2(type, message, dialog_handler, NULL, NULL);
    wait_dialog();
    return g_dialog_result == 1;
}

void ui_show_live(fpsu_lang lang, const char *message)
{
    (void)lang;
    ui_begin_frame();
    ui_draw_shell("PSUF", "Safe mode", "FPSU");
    ui_draw_text(72, 140, message, UI_COLOR_TEXT, 2);
    ui_present();
    g_live_open = 1;
}

void ui_close_live(void)
{
    if (g_live_open) {
        g_live_open = 0;
    }
}

void ui_pump(void)
{
    sysUtilCheckCallback();
    /* The system overlay needs game flips even while it owns the controls. */
    if (ui_system_menu_open()) ui_refresh_frame();
}

int ui_exit_requested(void)
{
    return g_exit_requested;
}
#else
int ui_init(void)
{
    int i; g_color_pitch = g_width * 4; update_logical_viewport();
    for (i = 0; i < 2; ++i) g_color_buffers[i] = calloc((size_t)g_width * g_height, 4);
    load_ui_assets(); ui_load_theme(); g_ready = 1; return 0;
}
void ui_shutdown(void)
{
    free(g_font_data); free(g_brand_pixels);
    free(g_animations[0].data); free(g_animations[1].data);
}
void ui_pump(void) { }
int ui_exit_requested(void) { return 0; }
#endif

int ui_system_menu_open(void)
{
    return g_system_menu_open || g_system_drawing;
}

int ui_screen_width(void)
{
    return UI_LOGICAL_WIDTH;
}

int ui_screen_height(void)
{
    return UI_LOGICAL_HEIGHT;
}

void ui_fill_rect(int x, int y, int w, int h, unsigned int color)
{
    int x1, y1, x2, y2;
    if (!g_color_buffers[g_draw_buffer] || w <= 0 || h <= 0) {
        return;
    }
    if (x + w <= 0 || y + h <= 0 || x >= UI_LOGICAL_WIDTH || y >= UI_LOGICAL_HEIGHT) {
        return;
    }
    x1 = logical_to_physical_x_floor(x);
    y1 = logical_to_physical_y_floor(y);
    x2 = logical_to_physical_x_ceil(x + w);
    y2 = logical_to_physical_y_ceil(y + h);
    if (x2 <= x1) {
        x2 = x1 + 1;
    }
    if (y2 <= y1) {
        y2 = y1 + 1;
    }
    raw_fill_rect(x1, y1, x2 - x1, y2 - y1, surface_color(color));
}

void ui_draw_disc(int cx, int cy, int r, unsigned int color)
{
    int x, y;
    int pcx, pcy, rx, ry;
    long long limit;
    if (r <= 0) {
        return;
    }
    color = text_color(color);
    pcx = logical_to_physical_x_nearest(cx);
    pcy = logical_to_physical_y_nearest(cy);
    rx = (r * g_view_w + UI_LOGICAL_WIDTH / 2) / UI_LOGICAL_WIDTH;
    ry = (r * g_view_h + UI_LOGICAL_HEIGHT / 2) / UI_LOGICAL_HEIGHT;
    if (rx < 1) {
        rx = 1;
    }
    if (ry < 1) {
        ry = 1;
    }
    limit = (long long)rx * rx * ry * ry;
    for (y = -ry; y <= ry; ++y) {
        for (x = -rx; x <= rx; ++x) {
            long long v = (long long)x * x * ry * ry + (long long)y * y * rx * rx;
            if (v <= limit) {
                put_pixel(pcx + x, pcy + y, color);
            }
        }
    }
}

void ui_draw_round_rect(int x, int y, int w, int h, int radius, unsigned int color)
{
    int row;
    if (w <= 0 || h <= 0) {
        return;
    }
    if (radius < 1) {
        ui_fill_rect(x, y, w, h, color);
        return;
    }
    if (radius * 2 > w) {
        radius = w / 2;
    }
    if (radius * 2 > h) {
        radius = h / 2;
    }
    ui_fill_rect(x, y + radius, w, h - 2 * radius, color);
    for (row = 0; row < radius; ++row) {
        int dy = radius - row - 1;
        int inset = radius - (int)sqrtf((float)(radius * radius - dy * dy));
        ui_fill_rect(x + inset, y + row, w - inset * 2, 1, color);
        ui_fill_rect(x + inset, y + h - row - 1, w - inset * 2, 1, color);
    }
}

void ui_draw_panel(int x, int y, int w, int h, int radius, unsigned int color, unsigned int edge)
{
    if (w <= 0 || h <= 0) {
        return;
    }
    ui_draw_round_rect(x, y + 3, w, h, radius, 0xff08141eu);
    ui_draw_round_rect(x, y, w, h, radius, edge ? edge : color);
    if (edge) ui_draw_round_rect(x + 1, y + 1, w - 2, h - 2, radius > 1 ? radius - 1 : 0, color);
}

/* Interpolate in destination space so every output pixel is filled, including at 1080p. */
static inline u32 mix_pixel(u32 from, u32 to, unsigned int weight)
{
    unsigned int inverse = 256 - weight;
    u32 rb = (((from & 0xff00ffu) * inverse + (to & 0xff00ffu) * weight) >> 8) & 0xff00ffu;
    u32 g = (((from & 0x00ff00u) * inverse + (to & 0x00ff00u) * weight) >> 8) & 0x00ff00u;
    return 0xff000000u | rb | g;
}


static void draw_scaled_image(const void *pixels, unsigned int pitch, int source_x, int source_y,
    int source_w, int source_h, int x, int y, int w, int h, int backdrop)
{
    unsigned int positions[2048];
    unsigned int x_step, y_step, sy;
    int row, col;
    u32 *canvas = frame_pixels();
    if (!canvas || w < 1 || h < 1 || w > 2048 || x < 0 || y < 0 ||
        x + w > g_width || y + h > g_height || source_w < 1 || source_h < 1) return;
    x_step = w > 1 ? (((unsigned int)source_w - 1) << 16) / (unsigned int)(w - 1) : 0;
    y_step = h > 1 ? (((unsigned int)source_h - 1) << 16) / (unsigned int)(h - 1) : 0;
    for (col = 0; col < w; ++col) positions[col] = col * x_step;
    for (row = 0, sy = 0; row < h; ++row, sy += y_step) {
        unsigned int top_y = sy >> 16;
        unsigned int bottom_y = top_y + 1 < (unsigned int)source_h ? top_y + 1 : top_y;
        const u32 *top = (const u32 *)((const unsigned char *)pixels + (source_y + top_y) * pitch) + source_x;
        const u32 *bottom = (const u32 *)((const unsigned char *)pixels + (source_y + bottom_y) * pitch) + source_x;
        u32 *dest = canvas + (y + row) * (g_color_pitch / 4) + x;
        u32 tint = blend_rgb(surface_color(UI_COLOR_BG_TOP), surface_color(UI_COLOR_BG_BOTTOM), row, h);
        for (col = 0; col < w; ++col) {
            unsigned int left = positions[col] >> 16;
            unsigned int right = left + 1 < (unsigned int)source_w ? left + 1 : left;
            unsigned int frac = (positions[col] >> 8) & 255;
            u32 upper = mix_pixel(top[left], top[right], frac);
            u32 lower = mix_pixel(bottom[left], bottom[right], frac);
            u32 pixel = mix_pixel(upper, lower, (sy >> 8) & 255);
            dest[col] = backdrop ? mix_pixel(tint, pixel, g_theme_mode != UI_THEME_NORMAL ? 70 : 112) : pixel;
        }
    }
}

void ui_cover_size(unsigned int width, unsigned int height, int max_w, int max_h, int *w, int *h)
{
    int physical_w = max_w * g_view_w / UI_LOGICAL_WIDTH;
    int physical_h = max_h * g_view_h / UI_LOGICAL_HEIGHT;
    if (!width || !height || max_w <= 0 || max_h <= 0) { *w = 0; *h = 0; return; }
    if ((unsigned long long)width * physical_h > (unsigned long long)height * physical_w)
        physical_h = (unsigned long long)physical_w * height / width;
    else
        physical_w = (unsigned long long)physical_h * width / height;
    *w = physical_w * UI_LOGICAL_WIDTH / g_view_w;
    *h = physical_h * UI_LOGICAL_HEIGHT / g_view_h;
}

void ui_draw_cover(const void *pixels, unsigned int pitch, unsigned int width, unsigned int height,
    int x, int y, int w, int h)
{
    int left, top, right, bottom, draw_w, draw_h;
    if (!pixels || !width || !height || width > 8192 || height > 8192 ||
        pitch < width * 4 || w <= 0 || h <= 0) return;
    left = logical_to_physical_x_nearest(x);
    top = logical_to_physical_y_nearest(y);
    right = logical_to_physical_x_nearest(x + w);
    bottom = logical_to_physical_y_nearest(y + h);
    draw_w = right - left;
    draw_h = draw_w * (int)height / (int)width;
    if (draw_h > bottom - top) {
        draw_h = bottom - top;
        draw_w = draw_h * (int)width / (int)height;
    }
    draw_scaled_image(pixels, pitch, 0, 0, width, height,
        left + (right - left - draw_w) / 2, top + (bottom - top - draw_h) / 2,
        draw_w, draw_h, 0);
}

void ui_draw_cover_banner(const void *pixels, unsigned int pitch, unsigned int width, unsigned int height)
{
    int crop_w = width;
    int crop_h = height;
    if (!pixels || !width || !height || width > 8192 || height > 8192 || pitch < width * 4) return;
    if ((int)width * g_height > (int)height * g_width)
        crop_w = height * g_width / g_height;
    else
        crop_h = width * g_height / g_width;
    draw_scaled_image(pixels, pitch, ((int)width - crop_w) / 2, ((int)height - crop_h) / 2,
        crop_w, crop_h, 0, 0, g_width, g_height, 1);
}

static void ui_draw_logo(int x, int y, int w, int h, int home)
{
    const ui_animation *animation = &g_animations[home && g_animations[1].data ? 1 : 0];
    if (g_record_brand_slots && g_brand_slot_count < 4) {
        ui_brand_slot *slot = &g_brand_slots[g_brand_slot_count++];
        slot->x = x; slot->y = y; slot->w = w; slot->h = h;
        slot->home = home;
    }
    if (animation->data) {
        size_t offset = 16 + (size_t)animation->count * 4 +
            (size_t)animation->frame * animation->width * animation->height * 4;
        ui_draw_cover(animation->data + offset, animation->width * 4,
            animation->width, animation->height, x, y, w, h);
        return;
    }
    if (g_brand_pixels) {
        ui_draw_cover(g_brand_pixels, g_brand_pitch, g_brand_width, g_brand_height,
            x, y, w, h);
        return;
    }
    ui_draw_disc(x + w / 2, y + h / 2, h / 3, UI_COLOR_ACCENT);
    ui_draw_disc(x + w / 2, y + h / 2, h / 4, 0xff102836u);
    ui_draw_disc(x + w / 2, y + h / 2, h / 8, UI_COLOR_ACCENT_2);
}

void ui_draw_brand_logo(int x, int y, int w, int h)
{
    ui_draw_logo(x, y, w, h, 0);
}

void ui_draw_home_logo(int x, int y, int w, int h)
{
    ui_draw_logo(x, y, w, h, 1);
}

static void ui_animate_brand(void)
{
    struct timeval tv;
    u64 now;
    int i, changed[2] = {0};
    if (!g_brand_slot_count) return;
    gettimeofday(&tv, NULL);
    now = (u64)tv.tv_sec * 1000 + (u64)tv.tv_usec / 1000;
    for (i = 0; i < 2; ++i) {
        ui_animation *animation = &g_animations[i];
        if (!animation->data) continue;
        if (!animation->next_frame)
            animation->next_frame = now + animation->delays[animation->frame];
        if (now < animation->next_frame) continue;
        animation->frame = (animation->frame + 1) % animation->count;
        animation->next_frame = now + animation->delays[animation->frame];
        changed[i] = 1;
    }
    for (i = 0; i < g_brand_slot_count; ++i) {
        const ui_brand_slot *slot = &g_brand_slots[i];
        if (changed[slot->home && g_animations[1].data ? 1 : 0])
            ui_draw_logo(slot->x, slot->y, slot->w, slot->h, slot->home);
    }
}

static void ui_draw_text_fallback(int x, int y, const char *text, unsigned int color, int scale)
{
    int start_x;
    int px, py, start_px;
    int cell_x, cell_y;
    int max_x;
    if (!text) {
        return;
    }
    if (scale < 1) {
        scale = 1;
    }
    start_x = x;
    px = logical_to_physical_x_nearest(x);
    py = logical_to_physical_y_nearest(y);
    start_px = px;
    cell_x = scaled_text_cell_x(scale);
    cell_y = scaled_text_cell_y(scale);
    max_x = g_view_x + g_view_w - 16;
    while (*text) {
        const u8 *glyph;
        int row, col;
        char c = *text++;

        if (c == '\r') {
            continue;
        }
        if (c == '\n') {
            x = start_x;
            px = start_px;
            py += 9 * cell_y;
            continue;
        }

        glyph = glyph_for(c);
        for (row = 0; row < 7; ++row) {
            for (col = 0; col < 5; ++col) {
                if (glyph[row] & (1 << (4 - col))) {
                    raw_fill_rect(px + col * cell_x, py + row * cell_y, cell_x, cell_y, color);
                }
            }
        }
        x += 6 * scale;
        px += 6 * cell_x;
        if (px > max_x || x > UI_LOGICAL_WIDTH - 32) {
            x = start_x;
            px = start_px;
            py += 9 * cell_y;
        }
    }
}

static unsigned int next_ui_char(const char **p)
{
    unsigned char c = (unsigned char)*(*p)++;
    if (c < 128) return c;
    if ((c & 0xe0) == 0xc0 && ((unsigned char)**p & 0xc0) == 0x80) {
        unsigned int n = ((c & 31u) << 6) | ((unsigned char)*(*p)++ & 63u); return n;
    }
    while (((unsigned char)**p & 0xc0) == 0x80) ++*p;
    return '?';
}

int ui_text_width(const char *text, int scale)
{
    int w = 0; unsigned int c;
    if (!text) return 0;
    if (scale < 1) scale = 1;
    if (scale > 4) scale = 4;
    while (*text) {
        c = next_ui_char(&text); if (c == '\n') break;
        if (c < 32 || c > 255) c = '?';
        w += g_font_data ? g_glyphs[scale - 1][c - 32].advance : 6 * scale;
    }
    return w;
}

static int glyph_advance(unsigned int code, int scale)
{
    if (code < 32 || code > 255) code = '?';
    return g_font_data ? g_glyphs[scale - 1][code - 32].advance : 6 * scale;
}

static void draw_glyph(int x, int y, unsigned int code, int scale, u32 color, int right)
{
    int x1, y1, x2, y2, col, row;
    unsigned int sx_step, sy_step, sy;
    const ui_glyph *glyph;
    u32 *canvas = frame_pixels();
    if (!canvas || code == ' ') return;
    if (code < 32 || code > 255) code = '?';
    if (!g_font_data) {
        char letter[2] = {(char)code, 0};
        ui_draw_text_fallback(x, y, letter, color, scale);
        return;
    }
    glyph = &g_glyphs[scale - 1][code - 32];
    x1 = logical_to_physical_x_nearest(x);
    y1 = logical_to_physical_y_nearest(y);
    x2 = logical_to_physical_x_nearest(x + glyph->w);
    y2 = logical_to_physical_y_nearest(y + glyph->h);
    if (x1 < 0 || y1 < 0 || x2 <= x1 || y2 <= y1 || y1 >= g_height || x1 >= g_width) return;
    sx_step = ((unsigned int)glyph->w << 16) / (unsigned int)(x2 - x1);
    sy_step = ((unsigned int)glyph->h << 16) / (unsigned int)(y2 - y1);
    if (x2 > right) x2 = right;
    if (x2 > g_width) x2 = g_width;
    if (y2 > g_height) y2 = g_height;
    for (row = y1, sy = 0; row < y2; ++row, sy += sy_step) {
        unsigned int sx = 0;
        const unsigned char *mask = glyph->mask + (sy >> 16) * glyph->w;
        unsigned int last_y = (sy + sy_step - 1) >> 16;
        const unsigned char *last_mask;
        unsigned int next_y = (sy >> 16) + 1;
        const unsigned char *next_mask;
        if (last_y >= glyph->h) last_y = glyph->h - 1;
        last_mask = glyph->mask + last_y * glyph->w;
        if (next_y >= glyph->h) next_y = glyph->h - 1;
        next_mask = glyph->mask + next_y * glyph->w;
        u32 *dest = canvas + row * (g_color_pitch / 4);
        for (col = x1; col < x2; ++col, sx += sx_step) {
            unsigned int opacity = mask[sx >> 16];
            /* Keep narrow strokes visible when text is reduced for SD output. */
            if (sx_step > 98304 || sy_step > 98304) {
                unsigned int last_x = (sx + sx_step - 1) >> 16;
                unsigned int sample;
                if (last_x >= glyph->w) last_x = glyph->w - 1;
                sample = mask[last_x]; if (sample > opacity) opacity = sample;
                sample = last_mask[sx >> 16]; if (sample > opacity) opacity = sample;
                sample = last_mask[last_x]; if (sample > opacity) opacity = sample;
            } else {
                unsigned int next_x = (sx >> 16) + 1;
                unsigned int fx = (sx >> 8) & 255, fy = (sy >> 8) & 255;
                unsigned int upper, lower;
                if (next_x >= glyph->w) next_x = glyph->w - 1;
                upper = opacity * (256 - fx) + mask[next_x] * fx;
                lower = next_mask[sx >> 16] * (256 - fx) + next_mask[next_x] * fx;
                opacity = (upper * (256 - fy) + lower * fy) >> 16;
            }
            if (opacity == 255) dest[col] = color;
            else if (opacity) dest[col] = mix_pixel(dest[col], color, opacity);
        }
    }
}

void ui_draw_text_fit(int x, int y, const char *text, unsigned int color, int scale, int max_width)
{
    int right;
    if (!text || max_width <= 0 || x >= UI_LOGICAL_WIDTH) return;
    if (scale < 1) scale = 1;
    if (scale > 4) scale = 4;
    if (max_width > UI_LOGICAL_WIDTH - 32 - x) max_width = UI_LOGICAL_WIDTH - 32 - x;
    if (max_width <= 0) return;
    color = text_color(color);
    right = logical_to_physical_x_nearest(x + max_width);
    while (*text && y < UI_LOGICAL_HEIGHT - 14) {
        int pen = x;
        int limit = x + max_width;
        int dots = 3 * glyph_advance('.', scale);
        int truncated = ui_text_width(text, scale) > max_width;
        if (truncated && dots <= max_width) limit -= dots;
        while (*text && *text != '\n') {
            unsigned int code = next_ui_char(&text);
            int advance;
            if (code == '\r') continue;
            advance = glyph_advance(code, scale);
            if (pen + advance > limit) break;
            draw_glyph(pen, y, code, scale, color, right);
            pen += advance;
        }
        if (truncated && dots <= max_width) {
            int n;
            for (n = 0; n < 3; ++n) {
                draw_glyph(pen, y, '.', scale, color, right);
                pen += dots / 3;
            }
        }
        while (*text && *text != '\n') ++text;
        if (*text == '\n') ++text;
        y += scale == 1 ? 22 : (scale == 2 ? 30 : 44);
    }
}

void ui_draw_text_box(int x, int y, const char *text, unsigned int color, int scale,
    int width, int line_height, int max_lines)
{
    int row;
    if (!text || width <= 0 || scale < 1 || scale > 4 || line_height <= 0) return;
    for (row = 0; row < max_lines && *text; ++row) {
        char line[512];
        const char *end = text;
        const char *space = NULL;
        int used = 0;
        size_t length;
        while (*end && *end != '\n' && end - text < (int)sizeof(line) - 8) {
            const char *next = end;
            unsigned int code = next_ui_char(&next);
            int advance = glyph_advance(code, scale);
            if (used + advance > width && end > text) break;
            if (code == ' ') space = end;
            used += advance;
            end = next;
        }
        if (*end && *end != '\n' && space && space > text) end = space;
        length = (size_t)(end - text);
        memcpy(line, text, length);
        text = end;
        while (*text == ' ' || *text == '\r') ++text;
        if (*text == '\n') ++text;
        if (*text && row == max_lines - 1) {
            memcpy(line + length, "...", 3);
            length += 3;
        }
        line[length] = '\0';
        ui_draw_text_fit(x, y + row * line_height, line, color, scale, width);
    }
}

void ui_draw_text(int x, int y, const char *text, unsigned int color, int scale)
{
    ui_draw_text_fit(x, y, text, color, scale, UI_LOGICAL_WIDTH - 32 - x);
}

static int ui_wait_for_flip(int wait)
{
#ifndef FPSU_UI_HOST
    int checks = 0;
    sysUtilCheckCallback();
    if (g_exit_requested) return 0;
    if (g_flip_pending) {
        while (gcmGetFlipStatus() != 0) {
            if (!wait || checks++ >= 80) return 0;
            usleep(500);
            sysUtilCheckCallback();
            if (g_exit_requested) return 0;
        }
        g_flip_pending = 0;
        {
            static int first_flip;
            if (first_flip == 0) ui_boot_trace("first_frame_displayed");
            if (first_flip == 1) ui_boot_trace("menu_displayed");
            if (first_flip < 2) ++first_flip;
        }
    }
#else
    (void)wait;
#endif
    return 1;
}

void ui_begin_frame(void)
{
    int y;
    if (!g_color_buffers[g_draw_buffer]) return;
    g_frame_writable = g_canvas != NULL || ui_wait_for_flip(1);
    g_brand_slot_count = 0;
    g_record_brand_slots = 1;
    for (y = 0; y < g_height; ++y) {
        raw_fill_rect(0, y, g_width, 1, blend_rgb(surface_color(UI_COLOR_BG_TOP),
            surface_color(UI_COLOR_BG_BOTTOM), y, g_height));
    }
}

int ui_resume_frame(void)
{
    return g_canvas != NULL;
}

#ifndef FPSU_UI_HOST
static int ui_submit_flip(int buffer)
{
    __sync_synchronize();
    gcmResetFlipStatus();
    if (gcmSetFlip(g_context, buffer) != 0) return 0;
    rsxFlushBuffer(g_context);
    g_flip_pending = 1;
    g_last_flip_buffer = buffer;
    g_draw_buffer = 1 - buffer;
    return 1;
}
#endif

void ui_present(void)
{
    if (!g_ready || !g_frame_writable || !ui_wait_for_flip(1)) return;
    g_record_brand_slots = 0;
    ui_animate_brand();
    /* Start with a clean image even if VSH has drawn an overlay into this buffer. */
    if (g_canvas) memcpy(g_color_buffers[g_draw_buffer], g_canvas, (size_t)g_color_pitch * g_height);
#ifndef FPSU_UI_HOST
    if (!ui_submit_flip(g_draw_buffer)) return;
    {
        static int frames;
        static u64 sample_start;
        if (frames < 9) {
            struct timeval tv;
            u64 now;
            gettimeofday(&tv, NULL);
            now = (u64)tv.tv_sec * 1000000 + (u64)tv.tv_usec;
            ++frames;
            if (frames == 3) sample_start = now;
            if (frames == 9 && now > sample_start) {
                char timing[80];
                snprintf(timing, sizeof(timing), "menu_frame_us=%u video=%dx%d",
                    (unsigned int)((now - sample_start) / 6), g_width, g_height);
                ui_boot_trace(timing);
            }
        }
    }
#endif
}

void ui_refresh_frame(void)
{
#ifndef FPSU_UI_HOST
    if (!g_ready || !ui_wait_for_flip(0)) return;
    if (g_canvas) {
        ui_present();
    } else if (ui_system_menu_open() && g_last_flip_buffer >= 0) {
        /* With no RAM canvas, resubmit the last complete frame without touching it. */
        ui_submit_flip(g_last_flip_buffer);
    }
#endif
}

void ui_draw_shell(const char *title, const char *subtitle, const char *tag)
{
    int right = UI_LOGICAL_WIDTH - 300;
    const char *visible_tag = tag;
    ui_draw_brand_logo(48, 22, 80, 46);
    ui_draw_text(144, 18, title, UI_COLOR_TEXT, 3);
    ui_draw_text_fit(146, 57, subtitle, UI_COLOR_MUTED, 1, 760);
    if (visible_tag) {
        ui_draw_panel(right, 30, 220, 34, 17, 0xff16303cu, 0xff28444fu);
        ui_draw_text_fit(right + 18, 36, visible_tag, UI_COLOR_ACCENT_2, 1, 185);
    }
}

void ui_draw_menu_item(int x, int y, int w, const char *title, const char *body, int selected, int enabled)
{
    const int h = 74;
    u32 panel = selected ? 0xff214b5au : 0xff163241u;
    u32 marker = selected ? UI_COLOR_ACCENT_2 : 0xff316071u;
    u32 title_color = enabled ? UI_COLOR_TEXT : UI_COLOR_DISABLED;
    u32 body_color = enabled ? UI_COLOR_MUTED : UI_COLOR_DISABLED;

    ui_draw_panel(x, y, w, h, 14, panel, selected ? UI_COLOR_ACCENT_2 : 0xff234956u);
    ui_draw_disc(x + 28, y + h / 2, selected ? 7 : 4, marker);
    ui_draw_text_fit(x + 54, y + 10, title, title_color, 2, w - 72);
    if (body && body[0]) {
        ui_draw_text_fit(x + 54, y + 42, body, body_color, 1, w - 72);
    }
}

void ui_draw_menu_item_slim(int x, int y, int w, const char *title, const char *body, int selected, int enabled)
{
    ui_draw_action_item(x, y, w, title, body, selected, enabled, UI_ICON_DOT);
}

void ui_draw_icon(int x, int y, ui_icon icon, unsigned int color, unsigned int background)
{
    int row;
    switch (icon) {
    case UI_ICON_UNLOCK:
        ui_draw_round_rect(x + 7, y, 12, 14, 6, color);
        ui_draw_round_rect(x + 9, y + 2, 8, 13, 4, background);
        ui_fill_rect(x + 15, y + 8, 7, 6, background);
        ui_draw_round_rect(x + 2, y + 10, 20, 14, 4, color);
        ui_draw_round_rect(x + 4, y + 12, 16, 10, 3, background);
        ui_draw_disc(x + 12, y + 17, 2, color);
        break;
    case UI_ICON_PLAY:
        for (row = 0; row < 20; ++row) {
            int width = 18 - 2 * abs(10 - row);
            ui_fill_rect(x + 5, y + row + 2, width, 1, color);
        }
        break;
    case UI_ICON_BACKUP:
        ui_draw_round_rect(x + 2, y + 2, 20, 20, 5, color);
        ui_draw_round_rect(x + 4, y + 4, 16, 16, 4, background);
        ui_fill_rect(x + 11, y + 5, 2, 10, color);
        ui_fill_rect(x + 8, y + 13, 8, 2, color);
        ui_fill_rect(x + 10, y + 15, 4, 2, color);
        break;
    case UI_ICON_LIBRARY:
        ui_draw_round_rect(x + 2, y + 5, 20, 15, 3, color);
        ui_draw_round_rect(x + 4, y + 8, 16, 10, 2, background);
        ui_fill_rect(x + 4, y + 3, 8, 4, color);
        break;
    case UI_ICON_HELP:
    case UI_ICON_DONATE:
        ui_draw_disc(x + 12, y + 12, 11, color);
        ui_draw_disc(x + 12, y + 12, 9, background);
        ui_draw_text(x + 8, y + 4, icon == UI_ICON_HELP ? "?" : "$", color, 1);
        break;
    case UI_ICON_CHIP:
        ui_draw_round_rect(x + 5, y + 5, 14, 14, 3, color);
        ui_draw_round_rect(x + 7, y + 7, 10, 10, 2, background);
        ui_fill_rect(x + 10, y + 10, 4, 4, color);
        for (row = 8; row <= 14; row += 3) {
            ui_fill_rect(x + row, y + 2, 2, 3, color);
            ui_fill_rect(x + row, y + 19, 2, 3, color);
            ui_fill_rect(x + 2, y + row, 3, 2, color);
            ui_fill_rect(x + 19, y + row, 3, 2, color);
        }
        break;
    case UI_ICON_UPDATE:
        ui_draw_disc(x + 12, y + 12, 9, color);
        ui_draw_disc(x + 12, y + 12, 7, background);
        ui_fill_rect(x + 1, y + 10, 5, 4, background);
        ui_fill_rect(x + 18, y + 10, 5, 4, background);
        for (row = 0; row < 4; ++row) {
            ui_fill_rect(x + 17 - row, y + 8 + row, 2 + row * 2, 1, color);
            ui_fill_rect(x + 5 - row, y + 15 - row, 2 + row * 2, 1, color);
        }
        break;
    case UI_ICON_MOON:
        ui_draw_disc(x + 11, y + 13, 9, color);
        ui_draw_disc(x + 15, y + 9, 8, background);
        break;
    case UI_ICON_EXIT: {
        static const unsigned char letters[4][5] = {
            {7, 4, 6, 4, 7}, {5, 5, 2, 5, 5},
            {7, 2, 2, 2, 7}, {7, 2, 2, 2, 2}
        };
        int letter, col;
        ui_draw_round_rect(x + 1, y + 5, 23, 14, 3, color);
        ui_draw_round_rect(x + 3, y + 7, 19, 10, 2, background);
        for (letter = 0; letter < 4; ++letter) {
            for (row = 0; row < 5; ++row) {
                for (col = 0; col < 3; ++col) {
                    if (letters[letter][row] & (4u >> col))
                        ui_fill_rect(x + 5 + 4 * letter + col, y + 9 + row, 1, 1, color);
                }
            }
        }
        break;
    }
    case UI_ICON_CROSS:
        /* PlayStation cross: diagonal strokes, not a plus sign. */
        for (row = 0; row < 12; ++row) {
            ui_fill_rect(x + 6 + row, y + 6 + row, 2, 2, color);
            ui_fill_rect(x + 16 - row, y + 6 + row, 2, 2, color);
        }
        break;
    case UI_ICON_CIRCLE:
        ui_draw_disc(x + 12, y + 12, 9, color);
        ui_draw_disc(x + 12, y + 12, 5, background);
        break;
    case UI_ICON_TRIANGLE:
        for (row = 0; row < 13; ++row) {
            int width = 2 + row * 2;
            ui_fill_rect(x + 11 - row, y + 5 + row, width, 1, color);
        }
        break;
    case UI_ICON_SQUARE:
        ui_draw_round_rect(x + 4, y + 4, 16, 16, 2, color);
        ui_draw_round_rect(x + 7, y + 7, 10, 10, 1, background);
        break;
    case UI_ICON_SELECT:
        ui_draw_round_rect(x + 3, y + 5, 18, 14, 3, color);
        ui_draw_round_rect(x + 6, y + 8, 12, 8, 2, background);
        ui_fill_rect(x + 8, y + 11, 8, 1, color);
        break;
    case UI_ICON_ARROWS:
        /* Four unmistakable navigation arrows around a small center. */
        ui_fill_rect(x + 11, y + 7, 2, 10, color);
        ui_fill_rect(x + 7, y + 11, 10, 2, color);
        ui_fill_rect(x + 10, y + 4, 4, 3, color);
        ui_fill_rect(x + 9, y + 5, 6, 2, color);
        ui_fill_rect(x + 10, y + 17, 4, 3, color);
        ui_fill_rect(x + 9, y + 17, 6, 2, color);
        ui_fill_rect(x + 4, y + 10, 3, 4, color);
        ui_fill_rect(x + 5, y + 9, 2, 6, color);
        ui_fill_rect(x + 17, y + 10, 3, 4, color);
        ui_fill_rect(x + 17, y + 9, 2, 6, color);
        break;
    default:
        ui_draw_disc(x + 12, y + 12, 4, color);
        break;
    }
}

void ui_draw_action_item(int x, int y, int w, const char *title, const char *body,
    int selected, int enabled, ui_icon icon)
{
    const int h = 58;
    u32 panel = selected ? 0xff214b5au : 0xff163241u;
    u32 marker = selected ? UI_COLOR_ACCENT_2 : 0xff316071u;
    u32 title_color = enabled ? UI_COLOR_TEXT : UI_COLOR_DISABLED;
    u32 body_color = enabled ? UI_COLOR_MUTED : UI_COLOR_DISABLED;

    ui_draw_panel(x, y, w, h, 13, panel, selected ? UI_COLOR_ACCENT_2 : 0xff234956u);
    ui_draw_icon(x + 15, y + (h - 24) / 2, icon, enabled ? marker : UI_COLOR_DISABLED, panel);
    ui_draw_text_fit(x + 52, y + 3, title, title_color, 2, w - 70);
    if (body && body[0]) {
        ui_draw_text_fit(x + 52, y + 32, body, body_color, 1, w - 70);
    }
}

void ui_draw_footer_actions(int x, int y, int width, const ui_footer_action *actions, int count)
{
    int i;
    int gap = 10;
    int item_width;
    if (!actions || count <= 0 || width <= 0) return;
    if (count > 5) count = 5;
    item_width = (width - gap * (count - 1)) / count;
    if (item_width < 60) item_width = 60;
    for (i = 0; i < count; ++i) {
        int item_x = x + i * (item_width + gap);
        const char *label = actions[i].label ? actions[i].label : "";
        ui_draw_panel(item_x, y, item_width, 34, 12, 0xff102631u, 0xff2d5b6au);
        ui_draw_icon(item_x + 8, y + 5, actions[i].icon, UI_COLOR_ACCENT_2, 0xff102631u);
        ui_draw_text_fit(item_x + 38, y + 9, label, UI_COLOR_TEXT, 1, item_width - 46);
    }
}

void ui_draw_footer(const char *left, const char *right)
{
    int y = UI_LOGICAL_HEIGHT - 48;
    ui_fill_rect(48, y - 12, 1184, 1, 0xff233e49u);
    if (left) {
        ui_draw_text_fit(54, y + 2, left, UI_COLOR_MUTED, 1, 620);
    }
    if (right) {
        ui_draw_text_fit(UI_LOGICAL_WIDTH - 510, y + 2, right, UI_COLOR_MUTED, 1, 460);
    }
}

int ui_scan_progress(const char *path, int found, void *user)
{
    (void)path;
    (void)found;
    (void)user;
    return 0;
}
