#ifndef CANVAS_H
#define CANVAS_H

#include <stdint.h>
#include "history.h"

typedef int Color;

#define RGB(r, g, b) (((r) << 16) | ((g) << 8) | (b))
#define COLOR_NONE (-1)

#define C_TEXT   RGB(236, 233, 226)
#define C_DIM    RGB(139, 134, 126)
#define C_FAINT  RGB(86, 83, 78)
#define C_ACCENT RGB(217, 119, 87)
#define C_AMBER  RGB(229, 181, 103)
#define C_ROSE   RGB(224, 108, 117)
#define C_SKY    RGB(122, 162, 214)
#define C_GREEN  RGB(126, 190, 148)
#define C_MAUVE  RGB(178, 148, 214)
#define C_TRACK  RGB(48, 46, 43)
#define C_SELECT RGB(66, 62, 57)

typedef struct {
    Color low;
    Color mid;
    Color high;
} Ramp;

extern const Ramp RAMP_HEAT;
extern const Ramp RAMP_GREEN;
extern const Ramp RAMP_AMBER;
extern const Ramp RAMP_SKY;
extern const Ramp RAMP_ORANGE;
extern const Ramp RAMP_MAUVE;

void canvas_begin(int cols, int rows);
void canvas_put(int x, int y, uint32_t ch, Color fg, Color bg, int bold);
int canvas_text(int x, int y, int maxw, Color fg, Color bg, int bold, const char *s);
int canvas_printf(int x, int y, int maxw, Color fg, Color bg, int bold, const char *fmt, ...);
void canvas_fill(int x, int y, int w, int h, Color bg);
void canvas_box(int x, int y, int w, int h, Color border);
Color color_mix(Color a, Color b, double t);
Color ramp_at(const Ramp *ramp, double t);
void canvas_bar(int x, int y, int w, double percent, const Ramp *ramp);
void canvas_graph(int x, int y, int w, int h, const History *hist, double max, const Ramp *ramp);
void canvas_present(int force_full);

#endif
