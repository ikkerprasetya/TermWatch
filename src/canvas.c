#include "canvas.h"
#include "term.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint32_t ch;
    int32_t fg;
    int32_t bg;
    uint32_t bold;
} Cell;

const Ramp RAMP_HEAT = { RGB(110, 190, 150), RGB(229, 181, 103), RGB(224, 108, 117) };
const Ramp RAMP_GREEN = { RGB(70, 140, 100), RGB(110, 190, 150), RGB(165, 224, 184) };
const Ramp RAMP_AMBER = { RGB(170, 130, 60), RGB(229, 181, 103), RGB(245, 212, 152) };
const Ramp RAMP_SKY = { RGB(80, 120, 180), RGB(122, 162, 214), RGB(176, 206, 242) };
const Ramp RAMP_ORANGE = { RGB(160, 80, 55), RGB(217, 119, 87), RGB(244, 176, 146) };
const Ramp RAMP_MAUVE = { RGB(120, 96, 160), RGB(178, 148, 214), RGB(214, 192, 240) };

static Cell *cur = NULL;
static Cell *old = NULL;
static int grid_w = 0;
static int grid_h = 0;
static int old_valid = 0;
static char *out = NULL;
static size_t out_len = 0;
static size_t out_cap = 0;

static int out_reserve(size_t extra) {
    if (out_len + extra <= out_cap) return 1;
    size_t cap = out_cap ? out_cap : 65536;
    while (cap < out_len + extra) cap *= 2;
    char *p = realloc(out, cap);
    if (!p) return 0;
    out = p;
    out_cap = cap;
    return 1;
}

static void out_bytes(const char *s, size_t n) {
    if (!out_reserve(n)) return;
    memcpy(out + out_len, s, n);
    out_len += n;
}

static void out_utf8(uint32_t cp) {
    char b[4];
    size_t n;
    if (cp < 0x80) {
        b[0] = (char)cp;
        n = 1;
    } else if (cp < 0x800) {
        b[0] = (char)(0xC0 | (cp >> 6));
        b[1] = (char)(0x80 | (cp & 0x3F));
        n = 2;
    } else if (cp < 0x10000) {
        b[0] = (char)(0xE0 | (cp >> 12));
        b[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        b[2] = (char)(0x80 | (cp & 0x3F));
        n = 3;
    } else {
        b[0] = (char)(0xF0 | (cp >> 18));
        b[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
        b[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
        b[3] = (char)(0x80 | (cp & 0x3F));
        n = 4;
    }
    out_bytes(b, n);
}

static int decode(const char *s, uint32_t *cp) {
    unsigned char c = (unsigned char)s[0];
    int n;
    uint32_t v;
    if (c < 0x80) {
        *cp = c;
        return 1;
    }
    if ((c & 0xE0) == 0xC0) {
        n = 2;
        v = c & 0x1F;
    } else if ((c & 0xF0) == 0xE0) {
        n = 3;
        v = c & 0x0F;
    } else if ((c & 0xF8) == 0xF0) {
        n = 4;
        v = c & 0x07;
    } else {
        *cp = '?';
        return 1;
    }
    for (int i = 1; i < n; i++) {
        unsigned char t = (unsigned char)s[i];
        if ((t & 0xC0) != 0x80) {
            *cp = '?';
            return i;
        }
        v = (v << 6) | (t & 0x3F);
    }
    *cp = v;
    return n;
}

static int is_wide(uint32_t cp) {
    return (cp >= 0x1100 && cp <= 0x115F) || (cp >= 0x2E80 && cp <= 0xA4CF) ||
           (cp >= 0xAC00 && cp <= 0xD7A3) || (cp >= 0xF900 && cp <= 0xFAFF) ||
           (cp >= 0xFE30 && cp <= 0xFE6F) || (cp >= 0xFF00 && cp <= 0xFF60) ||
           (cp >= 0xFFE0 && cp <= 0xFFE6) || (cp >= 0x1F300 && cp <= 0x1F64F) ||
           (cp >= 0x1F900 && cp <= 0x1F9FF) || (cp >= 0x20000 && cp <= 0x3FFFD);
}

void canvas_begin(int cols, int rows) {
    if (cols < 1 || rows < 1) return;
    if (cols != grid_w || rows != grid_h || !cur) {
        free(cur);
        free(old);
        cur = calloc((size_t)cols * (size_t)rows, sizeof(Cell));
        old = calloc((size_t)cols * (size_t)rows, sizeof(Cell));
        grid_w = cols;
        grid_h = rows;
        old_valid = 0;
        if (!cur || !old) {
            free(cur);
            free(old);
            cur = NULL;
            old = NULL;
            grid_w = 0;
            grid_h = 0;
            return;
        }
    }
    size_t total = (size_t)grid_w * (size_t)grid_h;
    for (size_t i = 0; i < total; i++) {
        cur[i].ch = ' ';
        cur[i].fg = -1;
        cur[i].bg = -1;
        cur[i].bold = 0;
    }
}

void canvas_put(int x, int y, uint32_t ch, Color fg, Color bg, int bold) {
    if (!cur || x < 0 || y < 0 || x >= grid_w || y >= grid_h) return;
    Cell *c = &cur[(size_t)y * (size_t)grid_w + (size_t)x];
    c->ch = ch;
    c->fg = fg;
    c->bg = bg;
    c->bold = bold ? 1u : 0u;
}

int canvas_text(int x, int y, int maxw, Color fg, Color bg, int bold, const char *s) {
    int drawn = 0;
    while (*s && drawn < maxw) {
        uint32_t cp;
        s += decode(s, &cp);
        if ((cp >= 0x300 && cp <= 0x36F) || (cp >= 0x200B && cp <= 0x200F) || cp == 0xFEFF) continue;
        if (cp < 32 || cp == 127 || (cp >= 0x80 && cp < 0xA0) || is_wide(cp)) cp = '?';
        canvas_put(x + drawn, y, cp, fg, bg, bold);
        drawn++;
    }
    return drawn;
}

int canvas_printf(int x, int y, int maxw, Color fg, Color bg, int bold, const char *fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    return canvas_text(x, y, maxw, fg, bg, bold, buf);
}

void canvas_fill(int x, int y, int w, int h, Color bg) {
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) canvas_put(x + i, y + j, ' ', COLOR_NONE, bg, 0);
    }
}

void canvas_box(int x, int y, int w, int h, Color border) {
    if (w < 2 || h < 2) return;
    canvas_put(x, y, 0x256D, border, COLOR_NONE, 0);
    canvas_put(x + w - 1, y, 0x256E, border, COLOR_NONE, 0);
    canvas_put(x, y + h - 1, 0x2570, border, COLOR_NONE, 0);
    canvas_put(x + w - 1, y + h - 1, 0x256F, border, COLOR_NONE, 0);
    for (int i = 1; i < w - 1; i++) {
        canvas_put(x + i, y, 0x2500, border, COLOR_NONE, 0);
        canvas_put(x + i, y + h - 1, 0x2500, border, COLOR_NONE, 0);
    }
    for (int j = 1; j < h - 1; j++) {
        canvas_put(x, y + j, 0x2502, border, COLOR_NONE, 0);
        canvas_put(x + w - 1, y + j, 0x2502, border, COLOR_NONE, 0);
    }
}

Color color_mix(Color a, Color b, double t) {
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;
    int ar = (a >> 16) & 255, ag = (a >> 8) & 255, ab = a & 255;
    int br = (b >> 16) & 255, bg = (b >> 8) & 255, bb = b & 255;
    int r = (int)(ar + (br - ar) * t + 0.5);
    int g = (int)(ag + (bg - ag) * t + 0.5);
    int bl = (int)(ab + (bb - ab) * t + 0.5);
    return RGB(r, g, bl);
}

Color ramp_at(const Ramp *ramp, double t) {
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;
    if (t < 0.5) return color_mix(ramp->low, ramp->mid, t * 2.0);
    return color_mix(ramp->mid, ramp->high, (t - 0.5) * 2.0);
}

void canvas_bar(int x, int y, int w, double percent, const Ramp *ramp) {
    static const uint32_t partial[8] = { ' ', 0x258F, 0x258E, 0x258D, 0x258C, 0x258B, 0x258A, 0x2589 };
    if (w < 1) return;
    if (percent < 0.0) percent = 0.0;
    if (percent > 100.0) percent = 100.0;

    int eighths = (int)(percent / 100.0 * w * 8.0 + 0.5);
    int full = eighths / 8;
    int rest = eighths % 8;
    double span = w > 1 ? (double)(w - 1) : 1.0;

    for (int i = 0; i < w; i++) {
        Color tone = ramp_at(ramp, (double)i / span);
        if (i < full) canvas_put(x + i, y, 0x2588, tone, COLOR_NONE, 0);
        else if (i == full && rest > 0) canvas_put(x + i, y, partial[rest], tone, C_TRACK, 0);
        else canvas_put(x + i, y, ' ', COLOR_NONE, C_TRACK, 0);
    }
}

void canvas_graph(int x, int y, int w, int h, const History *hist, double max, const Ramp *ramp) {
    static const unsigned left_bits[4] = { 0x40, 0x04, 0x02, 0x01 };
    static const unsigned right_bits[4] = { 0x80, 0x20, 0x10, 0x08 };
    if (w < 1 || h < 1 || max <= 0.0) return;

    for (int c = 0; c < w; c++) {
        int dots[2] = { -1, -1 };
        for (int s = 0; s < 2; s++) {
            int back = (w - 1 - c) * 2 + (1 - s);
            if (back >= hist->count) continue;
            double v = history_at(hist, back);
            int d = (int)(v / max * 4.0 * h + 0.5);
            if (d < 1) d = 1;
            if (d > 4 * h) d = 4 * h;
            dots[s] = d;
        }
        for (int r = 0; r < h; r++) {
            unsigned mask = 0;
            int top = 0;
            for (int s = 0; s < 2; s++) {
                if (dots[s] < 0) continue;
                int k = dots[s] - 4 * r;
                if (k > 4) k = 4;
                if (k <= 0) continue;
                if (k > top) top = k;
                for (int i = 0; i < k; i++) mask |= s == 0 ? left_bits[i] : right_bits[i];
            }
            if (!mask) continue;
            double t = ((double)r + top / 4.0) / (double)h;
            canvas_put(x + c, y + h - 1 - r, 0x2800 + mask, ramp_at(ramp, t), COLOR_NONE, 0);
        }
    }
}

void canvas_present(int force_full) {
    if (!cur) return;
    out_len = 0;
    int full = force_full || !old_valid;
    int emitted = 0;
    int last_fg = -2;
    int last_bg = -2;
    int last_bold = -1;
    char tmp[96];

    for (int y = 0; y < grid_h; y++) {
        Cell *row = cur + (size_t)y * (size_t)grid_w;
        Cell *prev = old + (size_t)y * (size_t)grid_w;
        if (!full && memcmp(row, prev, sizeof(Cell) * (size_t)grid_w) == 0) continue;
        emitted = 1;
        int n = snprintf(tmp, sizeof(tmp), "\033[%d;1H", y + 1);
        out_bytes(tmp, (size_t)n);
        for (int x = 0; x < grid_w; x++) {
            const Cell *c = &row[x];
            if (c->fg != last_fg || c->bg != last_bg || (int)c->bold != last_bold) {
                n = snprintf(tmp, sizeof(tmp), "\033[0%s", c->bold ? ";1" : "");
                if (c->fg >= 0) {
                    n += snprintf(tmp + n, sizeof(tmp) - (size_t)n, ";38;2;%d;%d;%d",
                                  (c->fg >> 16) & 255, (c->fg >> 8) & 255, c->fg & 255);
                }
                if (c->bg >= 0) {
                    n += snprintf(tmp + n, sizeof(tmp) - (size_t)n, ";48;2;%d;%d;%d",
                                  (c->bg >> 16) & 255, (c->bg >> 8) & 255, c->bg & 255);
                }
                n += snprintf(tmp + n, sizeof(tmp) - (size_t)n, "m");
                out_bytes(tmp, (size_t)n);
                last_fg = c->fg;
                last_bg = c->bg;
                last_bold = (int)c->bold;
            }
            out_utf8(c->ch);
        }
    }

    if (emitted || full) {
        out_bytes("\033[0m", 4);
        term_present(out, out_len, full);
    }
    memcpy(old, cur, sizeof(Cell) * (size_t)grid_w * (size_t)grid_h);
    old_valid = 1;
}
