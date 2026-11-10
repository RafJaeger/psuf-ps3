#include <errno.h>
#include <stdio.h>
#include <unistd.h>
static int gameos_rename(const char *source, const char *destination);
#define rename gameos_rename
#include "../../source/ui.c"
#undef rename

static int gameos_rename(const char *source, const char *destination)
{
    if (access(destination, F_OK) == 0) { errno = EEXIST; return -1; }
    return rename(source, destination);
}

int main(int argc, char **argv)
{
    if (argc < 2) return 1;
    if (strcmp(argv[1], "theme") == 0) {
        int result = 0;
        ui_load_theme();
        if (argc > 2) result = ui_set_dark(atoi(argv[2]));
        printf("%d %d\n", result, ui_is_dark());
        return 0;
    }
    if (strcmp(argv[1], "colors") == 0 && argc > 2) {
        g_dark_theme = atoi(argv[2]);
        printf("%08x %08x %08x\n", surface_color(UI_COLOR_BG_TOP),
            surface_color(UI_COLOR_PANEL), text_color(UI_COLOR_TEXT));
        return 0;
    }
    if (strcmp(argv[1], "cover") == 0 && argc == 6) {
        int w, h;
        g_width = atoi(argv[2]); g_height = atoi(argv[3]);
        update_logical_viewport();
        ui_cover_size(atoi(argv[4]), atoi(argv[5]), 156, 226, &w, &h);
        printf("%d %d\n", w, h);
        return 0;
    }
    if (strcmp(argv[1], "animation") == 0) {
        int i;
        if (ui_init() != 0 || !g_animations[0].data || !g_animations[1].data ||
            g_animations[0].count < 2 || g_animations[1].count < 2) return 2;
        ui_begin_frame();
        ui_draw_brand_logo(48, 22, 80, 46);
        ui_draw_home_logo(448, 340, 728, 182);
        g_record_brand_slots = 0;
        g_animations[0].next_frame = 1;
        g_animations[1].next_frame = ~(u64)0;
        ui_animate_brand();
        if (g_animations[0].frame != 1 || g_animations[1].frame != 0) return 3;
        g_animations[0].next_frame = ~(u64)0;
        g_animations[1].next_frame = 1;
        ui_animate_brand();
        if (g_animations[0].frame != 1 || g_animations[1].frame != 1) return 4;
        for (i = 0; i < 2; ++i) free(g_color_buffers[i]);
        ui_shutdown();
        puts("ok");
        return 0;
    }
    return 1;
}
