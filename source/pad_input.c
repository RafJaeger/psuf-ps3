#include "pad_input.h"

#include <io/pad.h>
#include <string.h>
#include <sys/time.h>
#include <sysutil/sysutil.h>

static u32 g_down_buttons;
static int g_active_pad = -1;
static u32 g_old_buttons = 0;
static u32 g_repeat_buttons = 0;
static u64 g_repeat_at = 0;

int pad_input_init(void)
{
    g_down_buttons = 0;
    g_active_pad = -1;
    g_old_buttons = 0;
    g_repeat_buttons = 0;
    g_repeat_at = 0;
    return ioPadInit(7);
}

void pad_input_shutdown(void)
{
    ioPadEnd();
}

static u32 pad_input_read_down(void)
{
    padInfo info;
    padData data;
    u32 buttons = 0;
    int i;

    sysUtilCheckCallback();

    if (ioPadGetInfo(&info) != 0) {
        return g_down_buttons;
    }

    for (i = 0; i < MAX_PADS; ++i) {
        if (info.status[i] & 1) break;
    }
    if (i != g_active_pad) {
        g_active_pad = i;
        g_down_buttons = 0;
    }
    if (i == MAX_PADS) return 0;

    /* A report with len == 0 means the last physical state is unchanged. */
    if (ioPadGetData(i, &data) != 0 || data.len < 4) {
        return g_down_buttons;
    }

    memcpy(&buttons, &data.button[2], sizeof(buttons));
    if (data.len >= 8) {
        if (data.ANA_L_V < 70) buttons |= FPSU_BUTTON_UP;
        if (data.ANA_L_V > 185) buttons |= FPSU_BUTTON_DOWN;
        if (data.ANA_L_H < 70) buttons |= FPSU_BUTTON_LEFT;
        if (data.ANA_L_H > 185) buttons |= FPSU_BUTTON_RIGHT;
    }
    g_down_buttons = buttons;
    return buttons;
}

u32 pad_input_read_pressed(void)
{
    u32 buttons = pad_input_read_down();
    u32 pressed = buttons & ~g_old_buttons;

    g_old_buttons = buttons;
    return pressed;
}

u32 pad_input_read_active(void)
{
    u32 buttons = pad_input_read_down();
    u32 pressed = buttons & ~g_old_buttons;
    u32 directions = buttons & (FPSU_BUTTON_UP | FPSU_BUTTON_DOWN | FPSU_BUTTON_LEFT | FPSU_BUTTON_RIGHT);
    struct timeval clock;
    gettimeofday(&clock, NULL);
    u64 now = (u64)clock.tv_sec * 1000 + (u64)clock.tv_usec / 1000;
    if (directions != g_repeat_buttons) {
        g_repeat_buttons = directions;
        g_repeat_at = now + 320;
    } else if (directions && now >= g_repeat_at) {
        pressed |= directions;
        g_repeat_at = now + 100;
    }

    g_old_buttons = buttons;
    return pressed;
}
