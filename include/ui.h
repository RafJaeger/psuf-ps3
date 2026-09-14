#ifndef FPSU_UI_H
#define FPSU_UI_H

#include "app.h"

#define UI_COLOR_BG_TOP      0xFF101927u
#define UI_COLOR_BG_BOTTOM   0xFF152229u
#define UI_COLOR_PANEL       0xDD172A3Au
#define UI_COLOR_PANEL_SOFT  0xAA213345u
#define UI_COLOR_ACCENT      0xFF22C7C7u
#define UI_COLOR_ACCENT_2    0xFFFFC857u
#define UI_COLOR_GREEN       0xFF59D98Eu
#define UI_COLOR_ORANGE      0xFFFF8A4Cu
#define UI_COLOR_RED         0xFFFF5C75u
#define UI_COLOR_TEXT        0xFFF2F7F7u
#define UI_COLOR_MUTED       0xFF9FB2BDu
#define UI_COLOR_DISABLED    0xFF667986u

int ui_init(void);
void ui_shutdown(void);
void ui_message(fpsu_lang lang, const char *message);
int ui_confirm(fpsu_lang lang, const char *message);
void ui_show_live(fpsu_lang lang, const char *message);
void ui_close_live(void);
void ui_pump(void);
int ui_exit_requested(void);
int ui_scan_progress(const char *path, int found, void *user);
int ui_screen_width(void);
int ui_screen_height(void);
void ui_begin_frame(void);
void ui_present(void);
void ui_fill_rect(int x, int y, int w, int h, unsigned int color);
void ui_draw_disc(int cx, int cy, int r, unsigned int color);
void ui_draw_text(int x, int y, const char *text, unsigned int color, int scale);
void ui_draw_shell(const char *title, const char *subtitle, const char *tag);
void ui_draw_menu_item(int x, int y, int w, const char *title, const char *body, int selected, int enabled);
void ui_draw_footer(const char *left, const char *right);

#endif
