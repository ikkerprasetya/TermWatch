#include "ui.h"
#include "term.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RESET  "\033[0m"
#define BOLD   "\033[1m"
#define ACCENT "\033[38;2;217;119;87m"
#define TEXT   "\033[38;2;236;233;226m"
#define DIM    "\033[38;2;139;134;126m"
#define FAINT  "\033[38;2;86;83;78m"
#define AMBER  "\033[38;2;229;181;103m"
#define ROSE   "\033[38;2;224;108;117m"
#define SKY    "\033[38;2;122;162;214m"
#define TRACK  "\033[48;2;48;46;43m"

#define MIN_WIDTH 56
#define MAX_WIDTH 100
#define HISTORY 256

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} Frame;

static double history[HISTORY];
static int history_count = 0;

static void frame_reset(Frame *f) {
    f->len = 0;
    if (f->data) f->data[0] = '\0';
}

static void frame_free(Frame *f) {
    free(f->data);
    f->data = NULL;
    f->len = f->cap = 0;
}

static void put(Frame *f, const char *fmt, ...) {
    va_list args, copy;
    va_start(args, fmt);
    va_copy(copy, args);
    int need = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    if (need < 0) {
        va_end(args);
        return;
    }
    size_t required = f->len + (size_t)need + 1;
    if (required > f->cap) {
        size_t cap = f->cap ? f->cap : 1024;
        while (cap < required) cap *= 2;
        f->data = realloc(f->data, cap);
        f->cap = cap;
    }
    vsnprintf(f->data + f->len, f->cap - f->len, fmt, args);
    f->len += (size_t)need;
    va_end(args);
}

static void repeat(Frame *f, const char *glyph, int count) {
    for (int i = 0; i < count; i++) put(f, "%s", glyph);
}

static int visible_width(const char *s) {
    int width = 0;
    for (; *s; s++) {
        if (*s == '\033') {
            while (*s && *s != 'm') s++;
            if (!*s) break;
            continue;
        }
        if (((unsigned char)*s & 0xC0) != 0x80) width++;
    }
    return width;
}

static const char *tone_for(double percent, const char *base) {
    if (percent >= 88.0) return ROSE;
    if (percent >= 70.0) return AMBER;
    return base;
}

static void bar(Frame *f, double percent, int width, const char *base) {
    static const char *partial[] = { " ", "▏", "▎", "▍", "▌", "▋", "▊", "▉" };
    if (percent < 0.0) percent = 0.0;
    if (percent > 100.0) percent = 100.0;

    int eighths = (int)(percent / 100.0 * width * 8.0 + 0.5);
    int full = eighths / 8;
    int rest = eighths % 8;

    put(f, "%s", tone_for(percent, base));
    repeat(f, "█", full);
    int used = full;
    if (used < width) {
        put(f, TRACK "%s", partial[rest]);
        used++;
    } else {
        put(f, TRACK);
    }
    put(f, "%*s" RESET, width - used, "");
}

static void sparkline(Frame *f, int width, const char *base) {
    static const char *levels[] = { "▁", "▂", "▃", "▄", "▅", "▆", "▇", "█" };
    int shown = history_count < width ? history_count : width;
    put(f, "%*s", width - shown, "");
    for (int i = history_count - shown; i < history_count; i++) {
        double v = history[i % HISTORY];
        int level = (int)(v / 100.0 * 7.99);
        if (level < 0) level = 0;
        if (level > 7) level = 7;
        put(f, "%s%s", tone_for(v, base), levels[level]);
    }
    put(f, RESET);
}

static void box_top(Frame *f, const char *border, const char *title, int width) {
    int dashes = width - 5 - visible_width(title);
    if (dashes < 0) dashes = 0;
    put(f, "%s╭─ " RESET "%s" RESET "%s ", border, title, border);
    repeat(f, "─", dashes);
    put(f, "╮" RESET "\033[K\n");
}

static void box_bottom(Frame *f, const char *border, int width) {
    put(f, "%s╰", border);
    repeat(f, "─", width - 2);
    put(f, "╯" RESET "\033[K\n");
}

static void box_row(Frame *f, const char *border, const char *left, const char *right, int width) {
    int right_width = right ? visible_width(right) : 0;
    int gap = width - 4 - visible_width(left) - right_width;
    if (gap < 0) gap = 0;
    put(f, "%s│" RESET " %s" RESET "%*s%s" RESET " %s│" RESET "\033[K\n",
        border, left, gap, "", right ? right : "", border);
}

static void blank_line(Frame *f) {
    put(f, "\033[K\n");
}

static void format_uptime(char *out, size_t size, long seconds) {
    long d = seconds / 86400;
    long h = (seconds % 86400) / 3600;
    long m = (seconds % 3600) / 60;
    long s = seconds % 60;
    if (d > 0) snprintf(out, size, "%ldd %02ldh %02ldm %02lds", d, h, m, s);
    else if (h > 0) snprintf(out, size, "%ldh %02ldm %02lds", h, m, s);
    else snprintf(out, size, "%ldm %02lds", m, s);
}

static void render_header(Frame *f, Frame *l, Frame *r, const SystemInfo *sys, int width) {
    char uptime[48];
    format_uptime(uptime, sizeof(uptime), sys->uptime_seconds);

    box_top(f, ACCENT, ACCENT "✻" RESET " " BOLD TEXT "TermWatch", width);

    frame_reset(l);
    frame_reset(r);
    put(l, BOLD TEXT "%s", sys->hostname);
    put(r, DIM "up " TEXT "%s", uptime);
    box_row(f, ACCENT, l->data, r->data, width);

    frame_reset(l);
    frame_reset(r);
    put(l, DIM "%s %s", sys->os_name, sys->kernel_version);
    put(r, DIM "load " TEXT "%.2f %.2f %.2f", sys->load[0], sys->load[1], sys->load[2]);
    box_row(f, ACCENT, l->data, r->data, width);

    box_bottom(f, ACCENT, width);
}

static void render_cpu(Frame *f, Frame *l, Frame *r, const CpuInfo *cpu, int width) {
    int inner = width - 4;
    char cores[24];
    snprintf(cores, sizeof(cores), "%d cores", cpu->cores);
    int model_max = inner - (int)strlen(cores) - 2;

    box_top(f, FAINT, ACCENT "●" RESET " " BOLD TEXT "CPU", width);

    frame_reset(l);
    frame_reset(r);
    put(l, DIM "%.*s", model_max, cpu->model[0] ? cpu->model : "Unknown processor");
    put(r, DIM "%s", cores);
    box_row(f, FAINT, l->data, r->data, width);

    frame_reset(l);
    frame_reset(r);
    bar(l, cpu->usage_percent, inner - 8, ACCENT);
    put(r, BOLD TEXT "%5.1f%%", cpu->usage_percent);
    box_row(f, FAINT, l->data, r->data, width);

    frame_reset(l);
    sparkline(l, inner, ACCENT);
    box_row(f, FAINT, l->data, NULL, width);

    box_bottom(f, FAINT, width);
}

static void render_memory(Frame *f, Frame *l, Frame *r, const MemoryInfo *mem, int width) {
    int inner = width - 4;
    const double gb = 1024.0 * 1024.0;
    double total = mem->total_kb / gb;
    double avail = mem->available_kb / gb;
    double used = total - avail;
    double percent = mem->total_kb > 0 ? used / total * 100.0 : 0.0;

    box_top(f, FAINT, SKY "●" RESET " " BOLD TEXT "Memory", width);

    frame_reset(l);
    frame_reset(r);
    put(l, TEXT "%.1f" DIM " / %.1f GB", used, total);
    put(r, DIM "in use");
    box_row(f, FAINT, l->data, r->data, width);

    frame_reset(l);
    frame_reset(r);
    bar(l, percent, inner - 8, SKY);
    put(r, BOLD TEXT "%5.1f%%", percent);
    box_row(f, FAINT, l->data, r->data, width);

    frame_reset(l);
    put(l, DIM "available " TEXT "%.1f" DIM "   cached " TEXT "%.1f" DIM "   buffers " TEXT "%.1f" DIM " GB",
        avail, mem->cached_kb / gb, mem->buffers_kb / gb);
    box_row(f, FAINT, l->data, NULL, width);

    box_bottom(f, FAINT, width);
}

void ui_push_cpu_sample(double percent) {
    history[history_count % HISTORY] = percent;
    history_count++;
    if (history_count >= 2 * HISTORY) history_count -= HISTORY;
}

void ui_render(const SystemInfo *sys, const CpuInfo *cpu, const MemoryInfo *mem,
               int cols, int clear_all) {
    int width = cols < MIN_WIDTH ? MIN_WIDTH : (cols > MAX_WIDTH ? MAX_WIDTH : cols);

    Frame frame = {0}, left = {0}, right = {0};

    render_header(&frame, &left, &right, sys, width);
    blank_line(&frame);
    render_cpu(&frame, &left, &right, cpu, width);
    blank_line(&frame);
    render_memory(&frame, &left, &right, mem, width);
    blank_line(&frame);
    put(&frame, " " TEXT "q" DIM " quit   " FAINT "·" DIM "   updates every second" RESET "\033[K\n");

    term_present(frame.data, frame.len, clear_all);

    frame_free(&frame);
    frame_free(&left);
    frame_free(&right);
}
