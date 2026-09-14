#include "ui.h"

#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <rsx/gcm_sys.h>
#include <rsx/rsx.h>
#include <sysutil/msg.h>
#include <sysutil/sysutil.h>
#include <sysutil/video.h>

#define UI_CB_SIZE 0x100000
#define UI_HOST_SIZE (8 * 1024 * 1024)
#define UI_LOGICAL_WIDTH 1280
#define UI_LOGICAL_HEIGHT 720

static gcmContextData *g_context;
static void *g_host;
static u32 *g_color_buffers[2];
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
    u32 *target = g_color_buffers[g_draw_buffer];
    if (!target || x < 0 || y < 0 || x >= g_width || y >= g_height) {
        return;
    }
    target[(y * g_color_pitch / sizeof(u32)) + x] = color;
}

static void raw_fill_rect(int x, int y, int w, int h, unsigned int color)
{
    int xx, yy;
    u32 *target = g_color_buffers[g_draw_buffer];
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
        for (xx = x; xx < x + w; ++xx) {
            row[xx] = color;
        }
    }
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

static void sysutil_exit_callback(u64 status, u64 param, void *usrdata)
{
    (void)param;
    (void)usrdata;
    if (status == SYSUTIL_EXIT_GAME) {
        g_exit_requested = 1;
        g_dialog_result = 2;
    }
}

int ui_init(void)
{
    videoState state;
    videoResolution res;
    videoConfiguration config;

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

    sysUtilRegisterCallback(SYSUTIL_EVENT_SLOT0, sysutil_exit_callback, NULL);
    g_ready = 1;
    ui_begin_frame();
    ui_present();
    return 0;
}

void ui_shutdown(void)
{
    if (g_ready) {
        sysUtilUnregisterCallback(SYSUTIL_EVENT_SLOT0);
        rsxFinish(g_context, 1);
    }
    if (g_host) {
        free(g_host);
        g_host = NULL;
    }
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
}

int ui_exit_requested(void)
{
    return g_exit_requested;
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
    raw_fill_rect(x1, y1, x2 - x1, y2 - y1, color);
}

void ui_draw_disc(int cx, int cy, int r, unsigned int color)
{
    int x, y;
    int pcx, pcy, rx, ry;
    long long limit;
    if (r <= 0) {
        return;
    }
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

void ui_draw_text(int x, int y, const char *text, unsigned int color, int scale)
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

void ui_begin_frame(void)
{
    int y;
    if (!g_color_buffers[g_draw_buffer]) {
        return;
    }
    for (y = 0; y < g_height; ++y) {
        raw_fill_rect(0, y, g_width, 1, blend_rgb(UI_COLOR_BG_TOP, UI_COLOR_BG_BOTTOM, y, g_height));
    }
    ui_draw_disc(UI_LOGICAL_WIDTH - 140, 120, 72, 0xff203647u);
    ui_draw_disc(UI_LOGICAL_WIDTH - 72, UI_LOGICAL_HEIGHT - 96, 56, 0xff2a3d36u);
    ui_draw_disc(68, UI_LOGICAL_HEIGHT - 68, 42, 0xff24324du);
}

void ui_present(void)
{
    if (!g_ready) {
        return;
    }
    sysUtilCheckCallback();
    while (gcmGetFlipStatus() != 0) {
        usleep(200);
    }
    gcmResetFlipStatus();
    gcmSetFlip(g_context, g_draw_buffer);
    rsxFlushBuffer(g_context);
    g_draw_buffer = 1 - g_draw_buffer;
}

void ui_draw_shell(const char *title, const char *subtitle, const char *tag)
{
    int right = UI_LOGICAL_WIDTH - 300;
    ui_fill_rect(0, 0, UI_LOGICAL_WIDTH, 92, 0xff0d1824u);
    ui_fill_rect(0, 90, UI_LOGICAL_WIDTH, 2, UI_COLOR_ACCENT);
    ui_draw_disc(54, 46, 27, UI_COLOR_ACCENT);
    ui_draw_disc(54, 46, 18, 0xff0d1824u);
    ui_fill_rect(48, 27, 12, 38, UI_COLOR_ACCENT_2);
    ui_fill_rect(35, 40, 38, 12, UI_COLOR_ACCENT_2);
    ui_draw_text(96, 22, title, UI_COLOR_TEXT, 3);
    ui_draw_text(98, 56, subtitle, UI_COLOR_MUTED, 2);
    if (tag) {
        ui_fill_rect(right, 26, 220, 38, 0xff243445u);
        ui_draw_text(right + 16, 36, tag, UI_COLOR_ACCENT_2, 2);
    }
}

void ui_draw_menu_item(int x, int y, int w, const char *title, const char *body, int selected, int enabled)
{
    u32 panel = selected ? 0xff24465au : UI_COLOR_PANEL;
    u32 stripe = selected ? UI_COLOR_ACCENT_2 : UI_COLOR_ACCENT;
    u32 title_color = enabled ? UI_COLOR_TEXT : UI_COLOR_DISABLED;
    u32 body_color = enabled ? UI_COLOR_MUTED : UI_COLOR_DISABLED;

    ui_fill_rect(x, y, w, 74, panel);
    ui_fill_rect(x, y, 7, 74, stripe);
    if (selected) {
        ui_fill_rect(x + 7, y, w - 7, 2, UI_COLOR_ACCENT_2);
        ui_fill_rect(x + 7, y + 72, w - 7, 2, UI_COLOR_ACCENT_2);
    }
    ui_draw_text(x + 24, y + 14, title, title_color, 2);
    if (body && body[0]) {
        ui_draw_text(x + 24, y + 42, body, body_color, 1);
    }
}

void ui_draw_footer(const char *left, const char *right)
{
    int y = UI_LOGICAL_HEIGHT - 48;
    ui_fill_rect(0, y - 10, UI_LOGICAL_WIDTH, 58, 0xdd0d1824u);
    ui_fill_rect(0, y - 10, UI_LOGICAL_WIDTH, 2, 0xff2e4656u);
    if (left) {
        ui_draw_text(56, y + 4, left, UI_COLOR_MUTED, 2);
    }
    if (right) {
        ui_draw_text(UI_LOGICAL_WIDTH - 520, y + 4, right, UI_COLOR_MUTED, 2);
    }
}

int ui_scan_progress(const char *path, int found, void *user)
{
    (void)path;
    (void)found;
    (void)user;
    return 0;
}
