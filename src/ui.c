#include "ui.h"
#include "canvas.h"
#include "util.h"
#include <stdio.h>
#include <string.h>

#define MIN_COLS 60
#define MIN_ROWS 16
#define CORE_CELL 18
#define CORE_BAR 8
#define BORDER_MIX 0.5

#define INTERVAL_MIN 100
#define INTERVAL_MAX 10000
#define INTERVAL_STEP 100

typedef struct {
    int height;
    int core_cols;
    int core_rows;
    int area_w;
} CpuLayout;

typedef struct {
    char text[64];
    Color color;
} Segment;

void ui_init(UiState *ui) {
    memset(ui, 0, sizeof(*ui));
    ui->interval_ms = 1000;
    for (int i = 0; i < 4; i++) ui->show[i] = 1;
    pv_init(&ui->pv);
}

void ui_pick_interface(UiState *ui, const AppData *data) {
    unsigned long long best = 0;
    int pick = -1;
    for (int i = 0; i < data->net.count; i++) {
        const NetIface *n = &data->net.ifaces[i];
        if (strcmp(n->name, "lo") == 0) continue;
        unsigned long long total = n->rx + n->tx;
        if (pick < 0 || total > best) {
            pick = i;
            best = total;
        }
    }
    ui->net_sel = pick >= 0 ? pick : 0;
}

static int box_open(int x, int y, int w, int h, Color accent, const char *num, const char *name) {
    canvas_box(x, y, w, h, color_mix(accent, C_FAINT, BORDER_MIX));
    return x + 2 + canvas_printf(x + 2, y, w - 4, accent, COLOR_NONE, 1, " %s%s ", num, name);
}

static int title_right(int x, int y, int w, const Segment *segs, int n) {
    int total = 2;
    for (int i = 0; i < n; i++) total += utf8_width(segs[i].text) + (i ? 2 : 0);
    int start = x + w - 2 - total;
    canvas_fill(start, y, total, 1, COLOR_NONE);
    int cx = start + 1;
    for (int i = 0; i < n; i++) {
        if (i) cx += 2;
        cx += canvas_text(cx, y, w, segs[i].color, COLOR_NONE, 0, segs[i].text);
    }
    return start;
}

static CpuLayout cpu_layout(int cores, int cols, int rows) {
    CpuLayout L;
    int max_content = rows * 4 / 10 - 2;
    if (max_content < 6) max_content = 6;
    int cr = cores < max_content ? cores : max_content;
    if (cr < 1) cr = 1;
    int cc = (cores + cr - 1) / cr;
    int max_cc = (cols - 29) / CORE_CELL;
    if (max_cc < 1) max_cc = 1;
    if (cc > max_cc) {
        cc = max_cc;
        cr = (cores + cc - 1) / cc;
    }
    int used = cr < max_content ? cr : max_content;
    int min_content = rows >= 24 ? 7 : 6;
    int content = used < min_content ? min_content : used;
    if (content > max_content) content = max_content;
    int cap = rows - 1 - 8 - 2;
    if (content > cap) content = cap;
    if (content < 5) content = 5;
    L.height = content + 2;
    L.core_cols = cc;
    L.core_rows = used;
    L.area_w = cc * CORE_CELL - 1;
    return L;
}

static void draw_cpu(const AppData *d, int w, const CpuLayout *L) {
    const CpuInfo *cpu = &d->cpu;
    int x = 0;
    int y = 0;
    int h = L->height;
    int ch = h - 2;

    int tx = box_open(x, y, w, h, C_ACCENT, "\xc2\xb9", "cpu");

    Segment segs[3];
    int ns = 0;
    if (cpu->freq_mhz > 0.0) {
        if (cpu->freq_mhz >= 1000.0) snprintf(segs[ns].text, sizeof(segs[ns].text), "%.2f GHz", cpu->freq_mhz / 1000.0);
        else snprintf(segs[ns].text, sizeof(segs[ns].text), "%.0f MHz", cpu->freq_mhz);
        segs[ns].color = C_TEXT;
        ns++;
    }
    if (cpu->temp_c > 0.0) {
        snprintf(segs[ns].text, sizeof(segs[ns].text), "%.0f\xc2\xb0" "C", cpu->temp_c);
        segs[ns].color = ramp_at(&RAMP_HEAT, (cpu->temp_c - 35.0) / 55.0);
        ns++;
    }
    char up[48];
    format_uptime(up, sizeof(up), d->sys.uptime_seconds);
    snprintf(segs[ns].text, sizeof(segs[ns].text), "up %s", up);
    segs[ns].color = C_DIM;
    ns++;
    int rstart = title_right(x, y, w, segs, ns);

    int room = rstart - tx - 2;
    if (room > 6 && cpu->model[0]) canvas_printf(tx + 1, y, room, C_DIM, COLOR_NONE, 0, " %s ", cpu->model);

    int cores_x0 = x + w - 2 - L->area_w;
    int gx = x + 2;
    int gw = cores_x0 - x - 4;

    if (gw >= 12) {
        canvas_text(gx, y + 1, 3, C_TEXT, COLOR_NONE, 1, "CPU");
        int bar_w = gw - 4 - 7;
        if (bar_w > 2) canvas_bar(gx + 4, y + 1, bar_w, cpu->usage_percent, &RAMP_HEAT);
        canvas_printf(gx + gw - 6, y + 1, 6, C_TEXT, COLOR_NONE, 1, "%5.1f%%", cpu->usage_percent);

        int graph_h = ch - 3;
        if (graph_h >= 1) canvas_graph(gx, y + 2, gw, graph_h, &d->cpu_hist, 100.0, &RAMP_HEAT);

        canvas_printf(gx, y + ch - 1, gw, C_DIM, COLOR_NONE, 0, "load average");
        canvas_printf(gx + 13, y + ch - 1, gw - 13, C_TEXT, COLOR_NONE, 0, "%.2f %.2f %.2f",
                      d->sys.load[0], d->sys.load[1], d->sys.load[2]);
        canvas_printf(gx, y + ch, gw, C_DIM, COLOR_NONE, 0, "%s \xc2\xb7 %s %s", d->sys.hostname, d->sys.os_name,
                      d->sys.kernel_version);
    }

    for (int i = 0; i < cpu->cores; i++) {
        int col = i / L->core_rows;
        int row = i % L->core_rows;
        if (col >= L->core_cols || row >= ch) break;
        int cx = cores_x0 + col * CORE_CELL;
        int cy = y + 1 + row;
        double u = cpu->core_usage[i];
        canvas_printf(cx, cy, 3, C_DIM, COLOR_NONE, 0, "C%-2d", cpu->core_id[i]);
        canvas_bar(cx + 4, cy, CORE_BAR, u, &RAMP_HEAT);
        canvas_printf(cx + 4 + CORE_BAR + 1, cy, 4, ramp_at(&RAMP_HEAT, u / 100.0), COLOR_NONE, 0, "%3.0f%%", u);
    }
}

static void metric_line(int x, int y, int iw, const char *label, unsigned long kb, double percent, const Ramp *ramp) {
    char value[16];
    format_bytes(value, sizeof(value), (double)kb * 1024.0);
    canvas_text(x, y, 10, C_DIM, COLOR_NONE, 0, label);
    int bar_w = iw - 22;
    if (bar_w >= 3) canvas_bar(x + 10, y, bar_w, percent, ramp);
    canvas_printf(x + iw - 11, y, 6, C_TEXT, COLOR_NONE, 0, "%6s", value);
    canvas_printf(x + iw - 4, y, 4, C_DIM, COLOR_NONE, 0, "%3.0f%%", percent);
}

static void rule_line(int x, int y, int iw, const char *label) {
    int lw = canvas_printf(x, y, iw, C_FAINT, COLOR_NONE, 0, "\xe2\x94\x80\xe2\x94\x80 ");
    lw += canvas_text(x + lw, y, iw - lw, C_DIM, COLOR_NONE, 0, label);
    lw += canvas_text(x + lw, y, iw - lw, C_FAINT, COLOR_NONE, 0, " ");
    for (int i = lw; i < iw; i++) canvas_put(x + i, y, 0x2500, C_FAINT, COLOR_NONE, 0);
}

static int mem_lines_needed(const AppData *d) {
    int n = 4;
    if (d->mem.swap_total_kb > 0) n += 3;
    if (d->disk_count > 0) n += 1 + 2 * d->disk_count;
    return n;
}

static void draw_mem(const AppData *d, int x, int y, int w, int h) {
    const MemoryInfo *m = &d->mem;
    int tx = box_open(x, y, w, h, C_SKY, "\xc2\xb2", "mem");
    char total[16];
    format_bytes(total, sizeof(total), (double)m->total_kb * 1024.0);
    Segment seg;
    snprintf(seg.text, sizeof(seg.text), "total %s", total);
    seg.color = C_DIM;
    (void)tx;
    title_right(x, y, w, &seg, 1);

    int ix = x + 2;
    int iw = w - 4;
    int ih = h - 2;
    int line = 0;
    double tot = m->total_kb > 0 ? (double)m->total_kb : 1.0;
    unsigned long used = m->total_kb - m->available_kb;

    if (line < ih) metric_line(ix, y + 1 + line++, iw, "Used", used, used / tot * 100.0, &RAMP_HEAT);
    if (line < ih) metric_line(ix, y + 1 + line++, iw, "Available", m->available_kb, m->available_kb / tot * 100.0, &RAMP_GREEN);
    if (line < ih) metric_line(ix, y + 1 + line++, iw, "Cached", m->cached_kb, m->cached_kb / tot * 100.0, &RAMP_AMBER);
    if (line < ih) metric_line(ix, y + 1 + line++, iw, "Free", m->free_kb, m->free_kb / tot * 100.0, &RAMP_SKY);

    if (m->swap_total_kb > 0) {
        double st = (double)m->swap_total_kb;
        unsigned long sused = m->swap_total_kb - m->swap_free_kb;
        char label[32];
        char sval[16];
        format_bytes(sval, sizeof(sval), st * 1024.0);
        snprintf(label, sizeof(label), "swap %s", sval);
        if (line < ih) rule_line(ix, y + 1 + line++, iw, label);
        if (line < ih) metric_line(ix, y + 1 + line++, iw, "Used", sused, sused / st * 100.0, &RAMP_HEAT);
        if (line < ih) metric_line(ix, y + 1 + line++, iw, "Free", m->swap_free_kb, m->swap_free_kb / st * 100.0, &RAMP_GREEN);
    }

    if (d->disk_count > 0 && line < ih) {
        rule_line(ix, y + 1 + line++, iw, "disks");
        for (int i = 0; i < d->disk_count && line + 1 < ih; i++) {
            const DiskInfo *dk = &d->disks[i];
            char used_s[16], total_s[16], sizes[40];
            format_bytes(used_s, sizeof(used_s), (double)dk->used);
            format_bytes(total_s, sizeof(total_s), (double)dk->total);
            snprintf(sizes, sizeof(sizes), "%s / %s", used_s, total_s);
            int sl = (int)strlen(sizes);
            int yy = y + 1 + line;
            canvas_text(ix, yy, iw - sl - 1, C_TEXT, COLOR_NONE, 1, dk->mount);
            canvas_text(ix + iw - sl, yy, sl, C_DIM, COLOR_NONE, 0, sizes);

            double denom = (double)(dk->used + dk->avail);
            double pct = denom > 0.0 ? (double)dk->used / denom * 100.0 : 0.0;
            char io[40] = "";
            if (dk->has_io && iw >= 30) {
                char r[16], wv[16];
                format_bytes(r, sizeof(r), dk->read_rate);
                format_bytes(wv, sizeof(wv), dk->write_rate);
                snprintf(io, sizeof(io), " R %s W %s", r, wv);
            }
            int iol = (int)strlen(io);
            int bar_w = iw - 5 - iol;
            if (bar_w >= 3) canvas_bar(ix, yy + 1, bar_w, pct, &RAMP_HEAT);
            canvas_printf(ix + bar_w + 1, yy + 1, 4, C_TEXT, COLOR_NONE, 0, "%3.0f%%", pct);
            if (iol) canvas_text(ix + iw - iol, yy + 1, iol, C_DIM, COLOR_NONE, 0, io);
            line += 2;
        }
    }
}

static void rate_line(int x, int y, int iw, const char *arrow, const char *name, Color tone, double rate,
                      double top, unsigned long long total) {
    char r[16], t[16], g[16], right[64];
    format_bytes(r, sizeof(r), rate);
    format_bytes(t, sizeof(t), top);
    format_bytes(g, sizeof(g), (double)total);
    snprintf(right, sizeof(right), "top %s/s  total %s", t, g);
    int rl = (int)strlen(right);
    int lw = canvas_printf(x, y, iw, tone, COLOR_NONE, 1, "%s %s", arrow, name);
    canvas_printf(x + lw + 1, y, iw - lw - 1, C_TEXT, COLOR_NONE, 1, "%s/s", r);
    if (iw - lw - 12 > rl) canvas_text(x + iw - rl, y, rl, C_DIM, COLOR_NONE, 0, right);
}

static void draw_net(const UiState *ui, const AppData *d, int x, int y, int w, int h) {
    int tx = box_open(x, y, w, h, C_MAUVE, "\xc2\xb3", "net");
    if (d->net.count == 0) {
        canvas_text(x + 2, y + 1, w - 4, C_DIM, COLOR_NONE, 0, "no interfaces");
        return;
    }
    const NetIface *n = &d->net.ifaces[ui->net_sel];
    canvas_printf(tx, y, w - 4, C_DIM, COLOR_NONE, 0, "\xe2\x80\xb9 %s \xe2\x80\xba ", n->name);
    if (n->ip[0]) {
        Segment seg;
        snprintf(seg.text, sizeof(seg.text), "%s", n->ip);
        seg.color = C_DIM;
        title_right(x, y, w, &seg, 1);
    }

    int ix = x + 2;
    int iw = w - 4;
    int ih = h - 2;
    if (ih < 2) return;
    unsigned long long rx_total = n->rx - n->rx_base;
    unsigned long long tx_total = n->tx - n->tx_base;

    if (ih < 5) {
        rate_line(ix, y + 1, iw, "\xe2\x96\xbc", "Download", C_SKY, n->rx_rate, n->rx_top, rx_total);
        rate_line(ix, y + 2, iw, "\xe2\x96\xb2", "Upload", C_ACCENT, n->tx_rate, n->tx_top, tx_total);
        return;
    }

    int dh = (ih - 2) / 2;
    int uh = ih - 2 - dh;
    int samples = iw * 2;
    double rx_max = history_peak(&n->rx_hist, samples);
    double tx_max = history_peak(&n->tx_hist, samples);
    if (rx_max < 10240.0) rx_max = 10240.0;
    if (tx_max < 10240.0) tx_max = 10240.0;

    rate_line(ix, y + 1, iw, "\xe2\x96\xbc", "Download", C_SKY, n->rx_rate, n->rx_top, rx_total);
    canvas_graph(ix, y + 2, iw, dh, &n->rx_hist, rx_max, &RAMP_SKY);
    rate_line(ix, y + 2 + dh, iw, "\xe2\x96\xb2", "Upload", C_ACCENT, n->tx_rate, n->tx_top, tx_total);
    canvas_graph(ix, y + 3 + dh, iw, uh, &n->tx_hist, tx_max, &RAMP_ORANGE);
}

static void draw_footer(const UiState *ui, int cols, int rows, long long now) {
    int y = rows - 1;
    char right[32];
    snprintf(right, sizeof(right), "update %dms", ui->interval_ms);
    int rl = (int)strlen(right);
    canvas_text(cols - rl - 1, y, rl, C_DIM, COLOR_NONE, 0, right);
    int room = cols - rl - 3;

    if (ui->message[0] && now < ui->message_until) {
        canvas_text(1, y, room, C_AMBER, COLOR_NONE, 0, ui->message);
        return;
    }

    static const char *hints[][2] = {
        { "q", "quit" }, { "h", "help" }, { "\xe2\x86\x90\xe2\x86\x92", "sort" }, { "e", "tree" },
        { "f", "filter" }, { "t", "term" }, { "k", "kill" }, { "s", "signal" },
        { "+-", "interval" }, { "1-4", "boxes" }
    };
    int cx = 1;
    for (size_t i = 0; i < sizeof(hints) / sizeof(hints[0]); i++) {
        int need = utf8_width(hints[i][0]) + 1 + utf8_width(hints[i][1]) + 2;
        if (cx + need > room) break;
        cx += canvas_text(cx, y, room, C_TEXT, COLOR_NONE, 1, hints[i][0]);
        cx += canvas_text(cx, y, room, C_DIM, COLOR_NONE, 0, " ");
        cx += canvas_text(cx, y, room, C_DIM, COLOR_NONE, 0, hints[i][1]);
        cx += 2;
    }
}

static void draw_help(int cols, int rows) {
    static const char *lines[][2] = {
        { "\xe2\x86\x91 \xe2\x86\x93 PgUp PgDn Home End", "move selection" },
        { "\xe2\x86\x90 \xe2\x86\x92", "change sort column" },
        { "r", "reverse sort order" },
        { "e", "toggle tree view" },
        { "space", "collapse or expand branch" },
        { "c", "toggle per-core cpu percent" },
        { "f", "filter (enter keep, esc clear)" },
        { "enter", "process details" },
        { "t / k / s", "terminate / kill / pick signal" },
        { "b / n", "previous / next interface" },
        { "z", "reset network totals" },
        { "+ / -", "change update interval" },
        { "1 2 3 4", "toggle cpu mem net proc" },
        { "mouse", "wheel, click row, click header" },
        { "h ? F1", "this help" },
        { "q", "quit" }
    };
    int n = (int)(sizeof(lines) / sizeof(lines[0]));
    int w = 62;
    int h = n + 4;
    if (cols < w + 2) w = cols - 2;
    if (rows < h + 2) h = rows - 2;
    if (w < 30 || h < 6) return;
    int x = (cols - w) / 2;
    int y = (rows - h) / 2;
    canvas_fill(x, y, w, h, COLOR_NONE);
    canvas_box(x, y, w, h, C_ACCENT);
    canvas_printf(x + 2, y, w - 4, C_ACCENT, COLOR_NONE, 1, " TermWatch help ");
    for (int i = 0; i < n && i < h - 4; i++) {
        canvas_text(x + 2, y + 1 + i, 26, C_TEXT, COLOR_NONE, 1, lines[i][0]);
        canvas_text(x + 29, y + 1 + i, w - 31, C_DIM, COLOR_NONE, 0, lines[i][1]);
    }
    canvas_text(x + 2, y + h - 2, w - 4, C_DIM, COLOR_NONE, 0, "press any key to close");
}

void ui_render(UiState *ui, const AppData *d, int cols, int rows, int clear_all, long long now) {
    canvas_begin(cols, rows);
    if (cols < MIN_COLS || rows < MIN_ROWS) {
        canvas_printf(1, 0, cols - 1, C_AMBER, COLOR_NONE, 1, "terminal too small");
        canvas_printf(1, 1, cols - 1, C_DIM, COLOR_NONE, 0, "need %dx%d, have %dx%d", MIN_COLS, MIN_ROWS, cols, rows);
        canvas_present(clear_all);
        return;
    }
    if (ui->net_sel >= d->net.count) ui->net_sel = d->net.count > 0 ? d->net.count - 1 : 0;
    pv_rebuild(&ui->pv, &d->procs);

    int top = 0;
    int avail = rows - 1;
    if (ui->show[0]) {
        CpuLayout L = cpu_layout(d->cpu.cores, cols, rows);
        draw_cpu(d, cols, &L);
        top = L.height;
    }

    int lower = avail - top;
    if (lower >= 6) {
        int mem_on = ui->show[1];
        int net_on = ui->show[2];
        int proc_on = ui->show[3];
        int left_on = mem_on || net_on;
        int lw = 0;
        if (left_on && proc_on) {
            lw = cols * 40 / 100;
            if (lw < 30) lw = 30;
            if (lw > 60) lw = 60;
            if (cols - lw < 46) left_on = 0;
        } else if (left_on) {
            lw = cols;
        }

        if (left_on) {
            if (mem_on && net_on && lower < 16) net_on = 0;
            if (mem_on && net_on) {
                int mh = mem_lines_needed(d) + 2;
                if (mh < 8) mh = 8;
                if (mh > lower - 8) mh = lower - 8;
                draw_mem(d, 0, top, lw, mh);
                draw_net(ui, d, 0, top + mh, lw, lower - mh);
            } else if (mem_on) {
                draw_mem(d, 0, top, lw, lower);
            } else {
                draw_net(ui, d, 0, top, lw, lower);
            }
        }
        if (proc_on) {
            int px = left_on ? lw : 0;
            pv_draw(&ui->pv, &d->procs, px, top, cols - px, lower, d->cpu.cores);
        }
    }

    draw_footer(ui, cols, rows, now);
    pv_draw_overlays(&ui->pv, &d->procs, cols, rows, d->sys.uptime_seconds);
    if (ui->pv.msg_pending) {
        snprintf(ui->message, sizeof(ui->message), "%s", ui->pv.msg);
        ui->message_until = now + 4000;
        ui->pv.msg_pending = 0;
    }
    if (ui->help) draw_help(cols, rows);
    canvas_present(clear_all);
}

int ui_key(UiState *ui, AppData *d, const TermKey *key, long long now) {
    int c = key->code;
    int result = 0;

    if (ui->help) {
        if (c == KEY_ESC || c == 'q' || c == 'h' || c == '?' || c == KEY_ENTER || c == KEY_F1) ui->help = 0;
        return 0;
    }

    int owned = ui->pv.filtering || ui->pv.detail || ui->pv.sigmenu;
    if (!owned) {
        switch (c) {
        case 'q':
        case 'Q':
            return UI_QUIT;
        case 'h':
        case '?':
        case KEY_F1:
            ui->help = 1;
            return 0;
        case '1':
        case '2':
        case '3':
        case '4':
            ui->show[c - '1'] = !ui->show[c - '1'];
            return 0;
        case '+':
        case '=':
        case '-':
        case '_':
            ui->interval_ms += (c == '+' || c == '=') ? INTERVAL_STEP : -INTERVAL_STEP;
            if (ui->interval_ms < INTERVAL_MIN) ui->interval_ms = INTERVAL_MIN;
            if (ui->interval_ms > INTERVAL_MAX) ui->interval_ms = INTERVAL_MAX;
            snprintf(ui->message, sizeof(ui->message), "update interval %d ms", ui->interval_ms);
            ui->message_until = now + 2000;
            return 0;
        case 'b':
            if (d->net.count > 0) ui->net_sel = (ui->net_sel + d->net.count - 1) % d->net.count;
            return 0;
        case 'n':
            if (d->net.count > 0) ui->net_sel = (ui->net_sel + 1) % d->net.count;
            return 0;
        case 'z':
            net_reset_totals(&d->net, ui->net_sel);
            return 0;
        default:
            break;
        }
    }

    pv_key(&ui->pv, &d->procs, key);
    if (ui->pv.msg_pending) {
        snprintf(ui->message, sizeof(ui->message), "%s", ui->pv.msg);
        ui->message_until = now + 4000;
        ui->pv.msg_pending = 0;
        result |= UI_RESAMPLE;
    }
    return result;
}
