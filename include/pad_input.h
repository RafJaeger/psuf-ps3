#ifndef FPSU_PAD_INPUT_H
#define FPSU_PAD_INPUT_H

#include <ppu-types.h>

#define FPSU_BUTTON_LEFT     0x00800000
#define FPSU_BUTTON_DOWN     0x00400000
#define FPSU_BUTTON_RIGHT    0x00200000
#define FPSU_BUTTON_UP       0x00100000
#define FPSU_BUTTON_START    0x00080000
#define FPSU_BUTTON_SELECT   0x00010000
#define FPSU_BUTTON_SQUARE   0x0080
#define FPSU_BUTTON_CROSS    0x0040
#define FPSU_BUTTON_CIRCLE   0x0020
#define FPSU_BUTTON_TRIANGLE 0x0010

int pad_input_init(void);
void pad_input_shutdown(void);
u32 pad_input_read_pressed(void);
u32 pad_input_read_active(void);

#endif
