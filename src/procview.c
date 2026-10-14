#include "procview.h"
#include "canvas.h"
#include "util.h"
#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#define BORDER_MIX 0.5

typedef struct {
    int number;
    const char *name;
} SignalEntry;

static const SignalEntry signal_table[] = {
    { SIGHUP, "SIGHUP" },   { SIGINT, "SIGINT" },   { SIGQUIT, "SIGQUIT" },
    { SIGKILL, "SIGKILL" }, { SIGUSR1, "SIGUSR1" }, { SIGUSR2, "SIGUSR2" },
    { SIGTERM, "SIGTERM" }, { SIGCONT, "SIGCONT" }, { SIGSTOP, "SIGSTOP" }
};
#define SIGNAL_COUNT ((int)(sizeof(signal_table) / sizeof(signal_table[0])))

static const char *column_titles[SORT_COUNT] = { "Pid", "Program", "Command", "Thr", "User", "Mem", "Cpu%" };

static const ProcInfo *cmp_items = NULL;
static int cmp_sort = SORT_CPU;
static int cmp_reverse = 0;

static void set_message(ProcView *pv, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(pv->msg, sizeof(pv->msg), fmt, args);
    va_end(args);
    pv->msg_pending = 1;
}

void pv_init(ProcView *pv) {
    memset(pv, 0, sizeof(*pv));
    pv->sort = SORT_CPU;
    pv->sel_pid = -1;
}

int pv_overlay_active(const ProcView *pv) {
    return pv->detail || pv->sigmenu || pv->filtering;
}

static int cmp_proc(const void *pa, const void *pb) {
    const ProcInfo *a = &cmp_items[*(const int *)pa];
    const ProcInfo *b = &cmp_items[*(const int *)pb];
    int r = 0;
    switch (cmp_sort) {
    case SORT_PID: r = (a->pid > b->pid) - (a->pid < b->pid); break;
    case SORT_NAME: r = strcasecmp(a->comm, b->comm); break;
    case SORT_CMD: r = strcasecmp(a->cmd ? a->cmd : "", b->cmd ? b->cmd : ""); break;
    case SORT_THREADS: r = (b->threads > a->threads) - (b->threads < a->threads); break;
    case SORT_USER: r = strcasecmp(a->user, b->user); break;
    case SORT_MEM: r = (b->rss > a->rss) - (b->rss < a->rss); break;
    case SORT_CPU: r = (b->cpu > a->cpu) - (b->cpu < a->cpu); break;
    default: break;
    }
    if (cmp_reverse) r = -r;
    if (r == 0) r = (a->pid > b->pid) - (a->pid < b->pid);
    return r;
}

static int contains_ci(const char *hay, const char *needle) {
    size_t nlen = strlen(needle);
    if (nlen == 0) return 1;
    for (; *hay; hay++) {
        if (strncasecmp(hay, needle, nlen) == 0) return 1;
    }
    return 0;
}

static int matches_filter(const ProcView *pv, const ProcInfo *p) {
    if (pv->filter_len == 0) return 1;
    char pid[16];
    snprintf(pid, sizeof(pid), "%d", p->pid);
    return contains_ci(p->comm, pv->filter) || contains_ci(p->cmd ? p->cmd : "", pv->filter) ||
           contains_ci(p->user, pv->filter) || contains_ci(pid, pv->filter);
}

static int find_index(const ProcList *pl, int pid) {
    int lo = 0;
    int hi = pl->count - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (pl->items[mid].pid == pid) return mid;
        if (pl->items[mid].pid < pid) lo = mid + 1;
        else hi = mid - 1;
    }
    return -1;
}

static int is_collapsed(const ProcView *pv, int pid) {
    for (int i = 0; i < pv->collapsed_count; i++) {
        if (pv->collapsed[i] == pid) return 1;
    }
    return 0;
}

static void toggle_collapsed(ProcView *pv, int pid) {
    for (int i = 0; i < pv->collapsed_count; i++) {
        if (pv->collapsed[i] == pid) {
            pv->collapsed[i] = pv->collapsed[--pv->collapsed_count];
            return;
        }
    }
    if (pv->collapsed_count >= pv->collapsed_cap) {
        int cap = pv->collapsed_cap ? pv->collapsed_cap * 2 : 16;
        int *grown = realloc(pv->collapsed, (size_t)cap * sizeof(int));
        if (!grown) return;
        pv->collapsed = grown;
        pv->collapsed_cap = cap;
    }
    pv->collapsed[pv->collapsed_count++] = pid;
}

typedef struct {
    ProcView *pv;
    const int *head;
    const int *next;
    const ProcList *pl;
} TreeCtx;

static void add_row(ProcView *pv, int idx, int has_children, int collapsed, const char *prefix) {
    if (pv->row_count >= pv->row_cap) return;
    PvRow *row = &pv->rows[pv->row_count++];
    row->idx = idx;
    row->has_children = has_children;
    row->collapsed = collapsed;
    snprintf(row->prefix, sizeof(row->prefix), "%s", prefix);
}

static void emit_tree(const TreeCtx *ctx, int idx, const char *base, int is_last, int depth, int is_root) {
    char prefix[128];
    char child_base[128];
    int has_children = ctx->head[idx] >= 0;
    int collapsed = has_children && is_collapsed(ctx->pv, ctx->pl->items[idx].pid);

    if (is_root) {
        prefix[0] = '\0';
        child_base[0] = '\0';
    } else {
        snprintf(prefix, sizeof(prefix), "%s%s", base, is_last ? "\xe2\x94\x94\xe2\x94\x80 " : "\xe2\x94\x9c\xe2\x94\x80 ");
        snprintf(child_base, sizeof(child_base), "%s%s", base, is_last ? "   " : "\xe2\x94\x82  ");
    }
    add_row(ctx->pv, idx, has_children, collapsed, prefix);
    if (!has_children || collapsed || depth >= 16) return;

    for (int c = ctx->head[idx]; c >= 0; c = ctx->next[c]) {
        emit_tree(ctx, c, child_base, ctx->next[c] < 0, depth + 1, 0);
    }
}

void pv_rebuild(ProcView *pv, const ProcList *pl) {
    int n = pl->count;
    if (n > pv->row_cap) {
        PvRow *grown = realloc(pv->rows, (size_t)n * sizeof(PvRow));
        if (!grown) return;
        pv->rows = grown;
        pv->row_cap = n;
    }
    pv->row_count = 0;

    if (n > 0) {
        char *keep = calloc((size_t)n, 1);
        int *order = malloc((size_t)n * sizeof(int));
        int *head = malloc((size_t)n * sizeof(int));
        int *next = malloc((size_t)n * sizeof(int));
        int *parent = malloc((size_t)n * sizeof(int));
        if (keep && order && head && next && parent) {
            for (int i = 0; i < n; i++) {
                keep[i] = (char)matches_filter(pv, &pl->items[i]);
                head[i] = -1;
                next[i] = -1;
                int p = find_index(pl, pl->items[i].ppid);
                parent[i] = p == i ? -1 : p;
            }
            if (pv->tree && pv->filter_len > 0) {
                for (int i = 0; i < n; i++) {
                    if (!(keep[i] == 1)) continue;
                    for (int p = parent[i]; p >= 0 && keep[p] == 0; p = parent[p]) keep[p] = 2;
                }
            }

            int m = 0;
            for (int i = 0; i < n; i++) {
                if (keep[i]) order[m++] = i;
            }
            cmp_items = pl->items;
            cmp_sort = pv->sort;
            cmp_reverse = pv->reverse;
            qsort(order, (size_t)m, sizeof(int), cmp_proc);

            if (!pv->tree) {
                for (int k = 0; k < m; k++) add_row(pv, order[k], 0, 0, "");
            } else {
                int root_head = -1;
                int *root_next = next;
                int *roots = malloc((size_t)n * sizeof(int));
                if (roots) {
                    for (int i = 0; i < n; i++) roots[i] = -1;
                    for (int k = m - 1; k >= 0; k--) {
                        int i = order[k];
                        int p = parent[i];
                        if (p >= 0 && keep[p]) {
                            next[i] = head[p];
                            head[p] = i;
                        } else {
                            roots[i] = root_head;
                            root_head = i;
                        }
                    }
                    TreeCtx ctx = { pv, head, root_next, pl };
                    for (int r = root_head; r >= 0; r = roots[r]) emit_tree(&ctx, r, "", 0, 0, 1);
                    free(roots);
                }
            }
        }
        free(keep);
        free(order);
        free(head);
        free(next);
        free(parent);
    }

    int found = -1;
    for (int r = 0; r < pv->row_count; r++) {
        if (pl->items[pv->rows[r].idx].pid == pv->sel_pid) {
            found = r;
            break;
        }
    }
    if (found >= 0) {
        pv->sel_row = found;
    } else if (pv->row_count > 0) {
        if (pv->sel_row >= pv->row_count) pv->sel_row = pv->row_count - 1;
        if (pv->sel_row < 0) pv->sel_row = 0;
        pv->sel_pid = pl->items[pv->rows[pv->sel_row].idx].pid;
    } else {
        pv->sel_row = 0;
        pv->sel_pid = -1;
    }
}

static const ProcInfo *selected(const ProcView *pv, const ProcList *pl) {
    if (pv->sel_row < 0 || pv->sel_row >= pv->row_count) return NULL;
    int idx = pv->rows[pv->sel_row].idx;
    if (idx < 0 || idx >= pl->count) return NULL;
    return &pl->items[idx];
}

static void move_selection(ProcView *pv, const ProcList *pl, int target) {
    if (pv->row_count == 0) return;
    if (target < 0) target = 0;
    if (target >= pv->row_count) target = pv->row_count - 1;
    pv->sel_row = target;
    int idx = pv->rows[target].idx;
    if (idx >= 0 && idx < pl->count) pv->sel_pid = pl->items[idx].pid;
}

static void send_signal(ProcView *pv, int pid, int sig, const char *name) {
    if (pid <= 1) {
        set_message(pv, "refusing to signal pid %d", pid);
        return;
    }
    if (kill(pid, sig) == 0) set_message(pv, "sent %s to pid %d", name, pid);
    else set_message(pv, "%s to pid %d failed: %s", name, pid, strerror(errno));
}

static int in_box(const ProcView *pv, int x, int y) {
    return x >= pv->box_x && x < pv->box_x + pv->box_w && y >= pv->box_y && y < pv->box_y + pv->box_h;
}

static int natural_descending(int sort) {
    return sort == SORT_THREADS || sort == SORT_MEM || sort == SORT_CPU;
}

int pv_key(ProcView *pv, const ProcList *pl, const TermKey *key) {
    int c = key->code;
    const ProcInfo *sel = selected(pv, pl);

    if (pv->sigmenu) {
        if (c == KEY_ESC || c == 'q') pv->sigmenu = 0;
        else if (c == KEY_UP && pv->sig_sel > 0) pv->sig_sel--;
        else if (c == KEY_DOWN && pv->sig_sel < SIGNAL_COUNT - 1) pv->sig_sel++;
        else if (c == KEY_ENTER) {
            send_signal(pv, pv->sig_pid, signal_table[pv->sig_sel].number, signal_table[pv->sig_sel].name);
            pv->sigmenu = 0;
        }
        return 1;
    }

    if (pv->detail) {
        if (c == KEY_ESC || c == KEY_ENTER || c == 'q' || c == 'i') pv->detail = 0;
        else if (c == 't') send_signal(pv, pv->detail_pid, SIGTERM, "SIGTERM");
        else if (c == 'k') send_signal(pv, pv->detail_pid, SIGKILL, "SIGKILL");
        else if (c == 's') {
            pv->sigmenu = 1;
            pv->sig_pid = pv->detail_pid;
        }
        return 1;
    }

    if (pv->filtering) {
        if (c == KEY_ESC) {
            pv->filtering = 0;
            pv->filter[0] = '\0';
            pv->filter_len = 0;
        } else if (c == KEY_ENTER) {
            pv->filtering = 0;
        } else if (c == KEY_BACKSPACE) {
            if (pv->filter_len > 0) pv->filter[--pv->filter_len] = '\0';
        } else if (c >= 32 && c < 127 && pv->filter_len < (int)sizeof(pv->filter) - 1) {
            pv->filter[pv->filter_len++] = (char)c;
            pv->filter[pv->filter_len] = '\0';
        } else if (c == KEY_UP) {
            move_selection(pv, pl, pv->sel_row - 1);
        } else if (c == KEY_DOWN) {
            move_selection(pv, pl, pv->sel_row + 1);
        }
        return 1;
    }

    int page = pv->list_h > 1 ? pv->list_h - 1 : 1;
    switch (c) {
    case KEY_UP: move_selection(pv, pl, pv->sel_row - 1); return 1;
    case KEY_DOWN: move_selection(pv, pl, pv->sel_row + 1); return 1;
    case KEY_PGUP: move_selection(pv, pl, pv->sel_row - page); return 1;
    case KEY_PGDN: move_selection(pv, pl, pv->sel_row + page); return 1;
    case KEY_HOME: move_selection(pv, pl, 0); return 1;
    case KEY_END: move_selection(pv, pl, pv->row_count - 1); return 1;
    case KEY_LEFT: pv->sort = (pv->sort + SORT_COUNT - 1) % SORT_COUNT; return 1;
    case KEY_RIGHT: pv->sort = (pv->sort + 1) % SORT_COUNT; return 1;
    case 'r': pv->reverse = !pv->reverse; return 1;
    case 'e': pv->tree = !pv->tree; return 1;
    case 'c': pv->per_core = !pv->per_core; return 1;
    case 'f': pv->filtering = 1; return 1;
    case KEY_ESC:
        pv->filter[0] = '\0';
        pv->filter_len = 0;
        return 1;
    case ' ':
        if (pv->tree && sel && pv->sel_row < pv->row_count && pv->rows[pv->sel_row].has_children) {
            toggle_collapsed(pv, sel->pid);
        }
        return 1;
    case KEY_ENTER:
    case 'i':
        if (sel) {
            pv->detail = 1;
            pv->detail_pid = sel->pid;
        }
        return 1;
    case 't':
        if (sel) send_signal(pv, sel->pid, SIGTERM, "SIGTERM");
        return 1;
    case 'k':
        if (sel) send_signal(pv, sel->pid, SIGKILL, "SIGKILL");
        return 1;
    case 's':
        if (sel) {
            pv->sigmenu = 1;
            pv->sig_pid = sel->pid;
            pv->sig_sel = 6;
        }
        return 1;
    case KEY_WHEEL_UP:
        if (in_box(pv, key->x, key->y)) move_selection(pv, pl, pv->sel_row - 3);
        return 1;
    case KEY_WHEEL_DOWN:
        if (in_box(pv, key->x, key->y)) move_selection(pv, pl, pv->sel_row + 3);
        return 1;
    case KEY_CLICK:
        if (!in_box(pv, key->x, key->y)) return 0;
        if (key->y == pv->head_y) {
            for (int s = 0; s < SORT_COUNT; s++) {
                if (pv->col_start[s] >= 0 && key->x >= pv->col_start[s] && key->x < pv->col_end[s]) {
                    if (pv->sort == s) pv->reverse = !pv->reverse;
                    else {
                        pv->sort = s;
                        pv->reverse = 0;
                    }
                    break;
                }
            }
        } else if (key->y >= pv->list_y && key->y < pv->list_y + pv->list_h) {
            move_selection(pv, pl, pv->scroll + (key->y - pv->list_y));
        }
        return 1;
    default:
        return 0;
    }
}

static void tone_for_percent(double value, Color *out) {
    *out = ramp_at(&RAMP_HEAT, value / 100.0);
}

void pv_draw(ProcView *pv, const ProcList *pl, int x, int y, int w, int h, int cores) {
    pv->box_x = x;
    pv->box_y = y;
    pv->box_w = w;
    pv->box_h = h;
    for (int s = 0; s < SORT_COUNT; s++) pv->col_start[s] = pv->col_end[s] = -1;
    pv->list_h = 0;
    if (w < 20 || h < 4) return;

    Color accent = C_GREEN;
    canvas_box(x, y, w, h, color_mix(accent, C_FAINT, BORDER_MIX));
    int tx = x + 2;
    tx += canvas_printf(tx, y, w - 4, accent, COLOR_NONE, 1, " \xe2\x81\xb4proc ");

    char summary[80];
    snprintf(summary, sizeof(summary), "%d tasks  %d thr  %d run", pl->count, pl->total_threads, pl->running);
    int slen = (int)strlen(summary);
    int sx = x + w - 4 - slen;
    if (sx > tx + 2) canvas_printf(sx, y, slen + 2, C_DIM, COLOR_NONE, 0, " %s ", summary);

    if (pv->filtering || pv->filter_len > 0) {
        int room = sx - tx - 2;
        if (room > 10) {
            canvas_printf(tx + 1, y, room, pv->filtering ? C_AMBER : C_DIM, COLOR_NONE, 0, " filter: %s%s ",
                          pv->filter, pv->filtering ? "\xe2\x96\x8f" : "");
        }
    }

    int bx = x + 2;
    bx += canvas_printf(bx, y + h - 1, w - 4, C_DIM, COLOR_NONE, 0, " ");
    bx += canvas_printf(bx, y + h - 1, w - 4, pv->tree ? C_ACCENT : C_DIM, COLOR_NONE, pv->tree, "e tree");
    bx += canvas_printf(bx, y + h - 1, w - 4, C_FAINT, COLOR_NONE, 0, " \xc2\xb7 ");
    bx += canvas_printf(bx, y + h - 1, w - 4, pv->reverse ? C_ACCENT : C_DIM, COLOR_NONE, pv->reverse, "r reverse");
    bx += canvas_printf(bx, y + h - 1, w - 4, C_FAINT, COLOR_NONE, 0, " \xc2\xb7 ");
    bx += canvas_printf(bx, y + h - 1, w - 4, pv->per_core ? C_ACCENT : C_DIM, COLOR_NONE, pv->per_core, "c per-core");
    canvas_printf(bx, y + h - 1, 2, C_DIM, COLOR_NONE, 0, " ");

    char count[40];
    snprintf(count, sizeof(count), " %d/%d ", pv->row_count ? pv->sel_row + 1 : 0, pv->row_count);
    int clen = (int)strlen(count);
    if (x + w - 3 - clen > bx + 4) canvas_text(x + w - 3 - clen, y + h - 1, clen, C_DIM, COLOR_NONE, 0, count);

    int iw = w - 4;
    int name_w = pv->tree ? 24 : 16;
    int cmd_w = 0;
    int show_cmd = 0;
    int show_thr_user = 0;
    if (iw - name_w - 38 >= 10) {
        show_cmd = 1;
        show_thr_user = 1;
        cmd_w = iw - name_w - 38;
    } else if (iw - name_w - 23 >= 10) {
        show_cmd = 1;
        cmd_w = iw - name_w - 23;
    } else {
        name_w = iw - 22;
        if (name_w < 4) name_w = 4;
    }

    int cx = x + 2;
    int pid_x = cx;
    cx += 8;
    int name_x = cx;
    cx += name_w + 1;
    int cmd_x = cx;
    if (show_cmd) cx += cmd_w + 1;
    int thr_x = cx;
    int user_x = cx;
    if (show_thr_user) {
        user_x = thr_x + 5;
        cx += 15;
    }
    int mem_x = cx;
    cx += 7;
    int cpu_x = cx;

    pv->col_start[SORT_PID] = pid_x;
    pv->col_end[SORT_PID] = pid_x + 7;
    pv->col_start[SORT_NAME] = name_x;
    pv->col_end[SORT_NAME] = name_x + name_w;
    if (show_cmd) {
        pv->col_start[SORT_CMD] = cmd_x;
        pv->col_end[SORT_CMD] = cmd_x + cmd_w;
    }
    if (show_thr_user) {
        pv->col_start[SORT_THREADS] = thr_x;
        pv->col_end[SORT_THREADS] = thr_x + 4;
        pv->col_start[SORT_USER] = user_x;
        pv->col_end[SORT_USER] = user_x + 9;
    }
    pv->col_start[SORT_MEM] = mem_x;
    pv->col_end[SORT_MEM] = mem_x + 6;
    pv->col_start[SORT_CPU] = cpu_x;
    pv->col_end[SORT_CPU] = cpu_x + 6;

    pv->head_y = y + 1;
    pv->list_y = y + 2;
    pv->list_h = h - 3;
    if (pv->list_h < 1) return;

    int right_aligned[SORT_COUNT] = { 1, 0, 0, 1, 0, 1, 1 };
    for (int s = 0; s < SORT_COUNT; s++) {
        if (pv->col_start[s] < 0) continue;
        int cw = pv->col_end[s] - pv->col_start[s];
        int active = s == pv->sort;
        char label[24];
        int desc = natural_descending(s) != pv->reverse;
        if (active) snprintf(label, sizeof(label), "%s%s", column_titles[s], desc ? "\xe2\x86\x93" : "\xe2\x86\x91");
        else snprintf(label, sizeof(label), "%s", column_titles[s]);
        int lw = utf8_width(label);
        int lx = right_aligned[s] && lw < cw ? pv->col_start[s] + cw - lw : pv->col_start[s];
        canvas_text(lx, pv->head_y, cw, active ? C_ACCENT : C_DIM, COLOR_NONE, active, label);
    }

    if (pv->sel_row < pv->scroll) pv->scroll = pv->sel_row;
    if (pv->sel_row >= pv->scroll + pv->list_h) pv->scroll = pv->sel_row - pv->list_h + 1;
    int max_scroll = pv->row_count - pv->list_h;
    if (max_scroll < 0) max_scroll = 0;
    if (pv->scroll > max_scroll) pv->scroll = max_scroll;
    if (pv->scroll < 0) pv->scroll = 0;

    for (int line = 0; line < pv->list_h; line++) {
        int r = pv->scroll + line;
        if (r >= pv->row_count) break;
        const PvRow *row = &pv->rows[r];
        if (row->idx < 0 || row->idx >= pl->count) continue;
        const ProcInfo *p = &pl->items[row->idx];
        int yy = pv->list_y + line;
        int is_sel = r == pv->sel_row;
        Color bg = is_sel ? C_SELECT : COLOR_NONE;
        if (is_sel) canvas_fill(x + 1, yy, w - 2, 1, C_SELECT);

        canvas_printf(pid_x, yy, 7, p->state == 'R' ? C_GREEN : C_DIM, bg, 0, "%7d", p->pid);

        int nx = name_x;
        int nroom = name_w;
        if (pv->tree) {
            int pw = canvas_text(nx, yy, nroom, C_FAINT, bg, 0, row->prefix);
            nx += pw;
            nroom -= pw;
            if (row->has_children && nroom > 0) {
                int mw = canvas_text(nx, yy, nroom, C_DIM, bg, 0, row->collapsed ? "[+] " : "[-] ");
                nx += mw;
                nroom -= mw;
            }
        }
        if (nroom > 0) canvas_text(nx, yy, nroom, C_TEXT, bg, is_sel, p->comm);

        if (show_cmd) canvas_text(cmd_x, yy, cmd_w, C_DIM, bg, 0, p->cmd ? p->cmd : "");
        if (show_thr_user) {
            canvas_printf(thr_x, yy, 4, C_DIM, bg, 0, "%4d", p->threads);
            canvas_text(user_x, yy, 9, C_DIM, bg, 0, p->user);
        }

        char mem[16];
        format_bytes(mem, sizeof(mem), (double)p->rss);
        canvas_printf(mem_x, yy, 6, C_TEXT, bg, 0, "%6s", mem);

        double core_value = p->cpu * cores;
        double shown = pv->per_core ? core_value : p->cpu;
        Color tone;
        tone_for_percent(core_value, &tone);
        if (core_value < 0.05) tone = C_FAINT;
        canvas_printf(cpu_x, yy, 6, tone, bg, 0, "%6.1f", shown);
    }
}

typedef struct {
    char label[16];
    char value[300];
} Pair;

static void pair_add(Pair *pairs, int *n, int max, const char *label, const char *fmt, ...) {
    if (*n >= max) return;
    snprintf(pairs[*n].label, sizeof(pairs[*n].label), "%s", label);
    va_list args;
    va_start(args, fmt);
    vsnprintf(pairs[*n].value, sizeof(pairs[*n].value), fmt, args);
    va_end(args);
    (*n)++;
}

static const char *state_name(char s) {
    switch (s) {
    case 'R': return "running";
    case 'S': return "sleeping";
    case 'D': return "disk sleep";
    case 'Z': return "zombie";
    case 'T': return "stopped";
    case 't': return "tracing stop";
    case 'I': return "idle";
    case 'X': return "dead";
    default: return "unknown";
    }
}

static void read_link(const char *path, char *out, size_t size) {
    ssize_t n = readlink(path, out, size - 1);
    if (n < 0) {
        snprintf(out, size, "-");
        return;
    }
    out[n] = '\0';
}

static void read_io(int pid, unsigned long long *rd, unsigned long long *wr, int *ok) {
    char path[48];
    char line[128];
    *ok = 0;
    snprintf(path, sizeof(path), "/proc/%d/io", pid);
    FILE *file = fopen(path, "r");
    if (!file) return;
    while (fgets(line, sizeof(line), file)) {
        if (sscanf(line, "read_bytes: %llu", rd) == 1) *ok |= 1;
        else if (sscanf(line, "write_bytes: %llu", wr) == 1) *ok |= 2;
    }
    fclose(file);
}

static void draw_detail(ProcView *pv, const ProcList *pl, int cols, int rows, long uptime) {
    int idx = find_index(pl, pv->detail_pid);
    if (idx < 0) {
        pv->detail = 0;
        set_message(pv, "process %d has exited", pv->detail_pid);
        return;
    }
    const ProcInfo *p = &pl->items[idx];
    Pair pairs[16];
    int n = 0;
    char a[32], b[32], path[64], link[256];
    long hz = sysconf(_SC_CLK_TCK);
    if (hz <= 0) hz = 100;
    int cores = 1;
    long online = sysconf(_SC_NPROCESSORS_ONLN);
    if (online > 0) cores = (int)online;

    pair_add(pairs, &n, 16, "PID", "%d", p->pid);
    pair_add(pairs, &n, 16, "Parent", "%d", p->ppid);
    pair_add(pairs, &n, 16, "User", "%s", p->user);
    pair_add(pairs, &n, 16, "State", "%c (%s)", p->state, state_name(p->state));
    pair_add(pairs, &n, 16, "Threads", "%d", p->threads);
    pair_add(pairs, &n, 16, "Nice", "%d", p->nice);
    pair_add(pairs, &n, 16, "CPU", "%.1f%% of total, %.1f%% of one core", p->cpu, p->cpu * cores);
    format_bytes(a, sizeof(a), (double)p->rss);
    format_bytes(b, sizeof(b), (double)p->vsize);
    pair_add(pairs, &n, 16, "Memory", "%s resident, %s virtual", a, b);
    format_clock(a, sizeof(a), (long)(p->ticks / (unsigned long long)hz));
    pair_add(pairs, &n, 16, "CPU time", "%s", a);
    long running = uptime - (long)(p->start / (unsigned long long)hz);
    if (running < 0) running = 0;
    format_clock(a, sizeof(a), running);
    pair_add(pairs, &n, 16, "Running", "%s", a);

    snprintf(path, sizeof(path), "/proc/%d/exe", p->pid);
    read_link(path, link, sizeof(link));
    pair_add(pairs, &n, 16, "Executable", "%s", link);
    snprintf(path, sizeof(path), "/proc/%d/cwd", p->pid);
    read_link(path, link, sizeof(link));
    pair_add(pairs, &n, 16, "Directory", "%s", link);

    unsigned long long rd = 0, wr = 0;
    int ok;
    read_io(p->pid, &rd, &wr, &ok);
    if (ok == 3) {
        format_bytes(a, sizeof(a), (double)rd);
        format_bytes(b, sizeof(b), (double)wr);
        pair_add(pairs, &n, 16, "Disk I/O", "%s read, %s written", a, b);
    }

    int w = cols - 4 < 80 ? cols - 4 : 80;
    int vw = w - 4 - 12;
    const char *cmd = p->cmd ? p->cmd : "";
    int cmd_len = utf8_width(cmd);
    int cmd_lines = vw > 0 ? (cmd_len + vw - 1) / vw : 1;
    if (cmd_lines < 1) cmd_lines = 1;
    if (cmd_lines > 4) cmd_lines = 4;
    int h = n + cmd_lines + 4;
    if (h > rows - 2) h = rows - 2;
    int x = (cols - w) / 2;
    int y = (rows - h) / 2;
    if (w < 30 || h < 6) return;

    canvas_fill(x, y, w, h, COLOR_NONE);
    canvas_box(x, y, w, h, C_ACCENT);
    canvas_printf(x + 2, y, w - 4, C_ACCENT, COLOR_NONE, 1, " %s ", p->comm);

    int line = y + 1;
    int last = y + h - 3;
    for (int i = 0; i < n && line <= last; i++, line++) {
        canvas_printf(x + 2, line, 11, C_DIM, COLOR_NONE, 0, "%s", pairs[i].label);
        canvas_text(x + 14, line, vw, C_TEXT, COLOR_NONE, 0, pairs[i].value);
    }
    if (line <= last) {
        canvas_printf(x + 2, line, 11, C_DIM, COLOR_NONE, 0, "Command");
        const char *s = cmd;
        for (int l = 0; l < cmd_lines && line <= last && *s; l++, line++) {
            int taken = canvas_text(x + 14, line, vw, C_TEXT, COLOR_NONE, 0, s);
            for (int k = 0; k < taken && *s; k++) {
                s++;
                while (((unsigned char)*s & 0xC0) == 0x80) s++;
            }
        }
    }
    canvas_printf(x + 2, y + h - 2, w - 4, C_DIM, COLOR_NONE, 0, "t terminate   k kill   s signal   esc close");
}

static void draw_sigmenu(const ProcView *pv, int cols, int rows) {
    int w = 26;
    int h = SIGNAL_COUNT + 4;
    if (cols < w + 2 || rows < h + 2) return;
    int x = (cols - w) / 2;
    int y = (rows - h) / 2;
    canvas_fill(x, y, w, h, COLOR_NONE);
    canvas_box(x, y, w, h, C_AMBER);
    canvas_printf(x + 2, y, w - 4, C_AMBER, COLOR_NONE, 1, " send signal to %d ", pv->sig_pid);
    for (int i = 0; i < SIGNAL_COUNT; i++) {
        int sel = i == pv->sig_sel;
        if (sel) canvas_fill(x + 1, y + 1 + i, w - 2, 1, C_SELECT);
        canvas_printf(x + 2, y + 1 + i, w - 4, sel ? C_TEXT : C_DIM, sel ? C_SELECT : COLOR_NONE, sel,
                      "%2d  %s", signal_table[i].number, signal_table[i].name);
    }
    canvas_printf(x + 2, y + h - 2, w - 4, C_DIM, COLOR_NONE, 0, "enter send   esc cancel");
}

void pv_draw_overlays(ProcView *pv, const ProcList *pl, int cols, int rows, long uptime) {
    if (pv->detail) draw_detail(pv, pl, cols, rows, uptime);
    if (pv->sigmenu) draw_sigmenu(pv, cols, rows);
}
