#include "pad_input.h"

#include <io/pad.h>
#include <string.h>
#include <sysutil/sysutil.h>

static u32 g_old_buttons = 0;
static u32 g_repeat_buttons = 0;
static int g_repeat_frames = 0;

int pad_input_init(void)
{
    g_old_buttons = 0;
    g_repeat_buttons = 0;
    g_repeat_frames = 0;
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
        return 0;
    }

    for (i = 0; i < MAX_PADS; ++i) {
        if (info.status[i]) {
            if (ioPadGetData(i, &data) == 0 && data.len > 0) {
                memcpy(&buttons, &data.button[2], sizeof(buttons));
                if (data.ANA_L_V < 70) {
                    buttons |= FPSU_BUTTON_UP;
                }
                if (data.ANA_L_V > 185) {
                    buttons |= FPSU_BUTTON_DOWN;
                }
                if (data.ANA_L_H < 70) {
                    buttons |= FPSU_BUTTON_LEFT;
                }
                if (data.ANA_L_H > 185) {
                    buttons |= FPSU_BUTTON_RIGHT;
                }
            }
            break;
        }
    }

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
    u32 active = pressed;

    if (buttons && buttons == g_repeat_buttons) {
        g_repeat_frames++;
        if (g_repeat_frames >= 18 && (g_repeat_frames % 6) == 0) {
            active |= buttons;
        }
    } else {
        g_repeat_buttons = buttons;
        g_repeat_frames = 0;
    }

    g_old_buttons = buttons;
    return active;
}
