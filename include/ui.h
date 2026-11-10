#ifndef FPSU_UI_H
#define FPSU_UI_H

#include "app.h"

#define UI_COLOR_BG_TOP      0xFF0A1724u
#define UI_COLOR_BG_BOTTOM   0xFF123142u
#define UI_COLOR_PANEL       0xFF173746u
#define UI_COLOR_PANEL_SOFT  0xFF1B4050u
#define UI_COLOR_PANEL_EDGE  0xFF2D5B6Au
#define UI_COLOR_PANEL_INNER 0xFF214A59u
#define UI_COLOR_ACCENT      0xFF22C7C7u
#define UI_COLOR_ACCENT_2    0xFFFFC857u
#define UI_COLOR_GREEN       0xFF59D98Eu
#define UI_COLOR_ORANGE      0xFFFF8A4Cu
#define UI_COLOR_RED         0xFFFF5C75u
#define UI_COLOR_TEXT        0xFFF2F7F7u
#define UI_COLOR_MUTED       0xFFB9CBD4u
#define UI_COLOR_DISABLED    0xFF667986u

int ui_init(void);
void ui_shutdown(void);
void ui_message(fpsu_lang lang, const char *message);
int ui_confirm(fpsu_lang lang, const char *message);
void ui_show_live(fpsu_lang lang, const char *message);
void ui_close_live(void);
void ui_pump(void);
int ui_exit_requested(void);
int ui_system_menu_open(void);
int ui_scan_progress(const char *path, int found, void *user);
int ui_screen_width(void);
int ui_screen_height(void);
void ui_begin_frame(void);
int ui_resume_frame(void);
void ui_present(void);
void ui_refresh_frame(void);
int ui_is_dark(void);
int ui_set_dark(int dark);
enum {
    UI_THEME_NORMAL = 0,
    UI_THEME_DARK = 1
};
int ui_theme_get(void);
int ui_set_theme(int theme);
void ui_cover_size(unsigned int width, unsigned int height, int max_w, int max_h, int *w, int *h);
void ui_fill_rect(int x, int y, int w, int h, unsigned int color);
void ui_draw_disc(int cx, int cy, int r, unsigned int color);
void ui_draw_round_rect(int x, int y, int w, int h, int radius, unsigned int color);
void ui_draw_panel(int x, int y, int w, int h, int radius, unsigned int color, unsigned int edge);
void ui_draw_cover(const void *pixels, unsigned int pitch, unsigned int width, unsigned int height,
    int x, int y, int w, int h);
void ui_draw_cover_banner(const void *pixels, unsigned int pitch, unsigned int width, unsigned int height);
void ui_draw_brand_logo(int x, int y, int w, int h);
void ui_draw_home_logo(int x, int y, int w, int h);
void ui_draw_text(int x, int y, const char *text, unsigned int color, int scale);
int ui_text_width(const char *text, int scale);
void ui_draw_text_fit(int x, int y, const char *text, unsigned int color, int scale, int max_width);
void ui_draw_text_box(int x, int y, const char *text, unsigned int color, int scale,
    int width, int line_height, int max_lines);
typedef enum {
    UI_ICON_DOT, UI_ICON_UNLOCK, UI_ICON_PLAY, UI_ICON_BACKUP, UI_ICON_LIBRARY,
    UI_ICON_HELP, UI_ICON_CHIP, UI_ICON_DONATE, UI_ICON_UPDATE, UI_ICON_MOON, UI_ICON_EXIT,
    UI_ICON_CROSS, UI_ICON_CIRCLE, UI_ICON_TRIANGLE, UI_ICON_SQUARE, UI_ICON_SELECT,
    UI_ICON_ARROWS
} ui_icon;
typedef struct {
    ui_icon icon;
    const char *label;
} ui_footer_action;
void ui_draw_icon(int x, int y, ui_icon icon, unsigned int color, unsigned int background);
void ui_draw_footer_actions(int x, int y, int width, const ui_footer_action *actions, int count);
void ui_draw_action_item(int x, int y, int w, const char *title, const char *body,
    int selected, int enabled, ui_icon icon);
void ui_draw_shell(const char *title, const char *subtitle, const char *tag);
void ui_draw_menu_item(int x, int y, int w, const char *title, const char *body, int selected, int enabled);
void ui_draw_menu_item_slim(int x, int y, int w, const char *title, const char *body, int selected, int enabled);
void ui_draw_footer(const char *left, const char *right);

#endif
