/* Exercise the real software renderer with the exact package assets. */
#include "../../source/ui.c"
#include "home_view.h"
static void le16(FILE *f, unsigned int n) { fputc(n & 255, f); fputc((n >> 8) & 255, f); }
static void le32(FILE *f, unsigned int n) { le16(f, n & 65535); le16(f, n >> 16); }
int main(int argc, char **argv)
{
    FILE *out; int x, y, i, selected = argc > 2 ? atoi(argv[2]) : 0;
    fpsu_lang lang = argc > 6 ? (fpsu_lang)atoi(argv[6]) : FPSU_LANG_PT;
    if (argc < 2) return 1;
    if (argc > 4) { g_width = atoi(argv[3]); g_height = atoi(argv[4]); }
    ui_init();
    if (!g_font_data || !g_brand_pixels) return 2;
    if (argc > 5) ui_set_dark(atoi(argv[5]));
    {
        /* A solid image must stay solid after enlargement. Catch holes at 1080p. */
        static const u32 sample[] = {0xffedbf70u, 0xffedbf70u, 0xffedbf70u, 0xffedbf70u};
        int left = logical_to_physical_x_nearest(100), top = logical_to_physical_y_nearest(100);
        int right = logical_to_physical_x_nearest(260), bottom = logical_to_physical_y_nearest(260);
        int size = right - left < bottom - top ? right - left : bottom - top;
        left += (right - left - size) / 2;
        top += (bottom - top - size) / 2;
        ui_begin_frame();
        ui_draw_cover(sample, 8, 2, 2, 100, 100, 160, 160);
        for (y = top; y < top + size; ++y)
            for (x = left; x < left + size; ++x)
                if (frame_pixels()[y * g_width + x] != sample[0]) return 4;
    }
    g_canvas = calloc((size_t)g_height, g_color_pitch);
    if (!g_canvas) return 5;
    for (i = 0; i < 48; ++i) {
        if (i == 0) {
            ui_begin_frame();
            ui_draw_shell("PSUF", "FPS Unlocker para PlayStation 3", "V3");
            ui_draw_footer("Setas  Navegar     X  Selecionar", "Triangle  Sobre     Circle  Sair");
        } else if (!ui_resume_frame()) {
            return 5;
        }
        home_view_draw(lang, i == 0 ? (selected + 10) % 11 : selected, i == 0);
        ui_present();
        if (memcmp(g_color_buffers[g_draw_buffer], g_canvas, (size_t)g_height * g_color_pitch) != 0)
            return 6;
        g_draw_buffer ^= 1;
    }
    g_draw_buffer ^= 1;
    ui_begin_frame();
    ui_draw_shell("PSUF", "FPS Unlocker para PlayStation 3", "V3");
    ui_draw_footer("Setas  Navegar     X  Selecionar", "Triangle  Sobre     Circle  Sair");
    home_view_draw(lang, selected, 1);
    if (memcmp(g_color_buffers[g_draw_buffer], g_canvas, (size_t)g_height * g_color_pitch) != 0) {
        int left = g_width, top = g_height, right = 0, bottom = 0;
        for (y = 0; y < g_height; ++y) for (x = 0; x < g_width; ++x) {
            int pixel = y * g_width + x;
            if (g_color_buffers[g_draw_buffer][pixel] == g_canvas[pixel]) continue;
            if (x < left) left = x;
            if (y < top) top = y;
            if (x > right) right = x;
            if (y > bottom) bottom = y;
        }
        fprintf(stderr, "Incremental frame differs from full frame at (%d,%d)-(%d,%d)\n",
            left, top, right, bottom);
        return 7;
    }
    out = fopen(argv[1], "wb"); if (!out) return 3;
    fputc('B', out); fputc('M', out); le32(out, 54 + g_width * g_height * 4);
    le32(out, 0); le32(out, 54); le32(out, 40); le32(out, g_width); le32(out, g_height);
    le16(out, 1); le16(out, 32); le32(out, 0); le32(out, g_width * g_height * 4);
    le32(out, 0); le32(out, 0); le32(out, 0); le32(out, 0);
    for (y = g_height - 1; y >= 0; --y) for (x = 0; x < g_width; ++x) le32(out, g_color_buffers[g_draw_buffer][y * g_width + x]);
    fclose(out);
    free(g_canvas);
    for (i = 0; i < 2; ++i) free(g_color_buffers[i]);
    ui_shutdown(); return 0;
}
