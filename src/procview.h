#ifndef PROCVIEW_H
#define PROCVIEW_H

#include "proc_info.h"
#include "term.h"

enum {
    SORT_PID,
    SORT_NAME,
    SORT_CMD,
    SORT_THREADS,
    SORT_USER,
    SORT_MEM,
    SORT_CPU,
    SORT_COUNT
};

typedef struct {
    int idx;
    int has_children;
    int collapsed;
    char prefix[128];
} PvRow;

typedef struct {
    int sort;
    int reverse;
    int tree;
    int per_core;
    char filter[64];
    int filter_len;
    int filtering;
    int sel_pid;
    int sel_row;
    int scroll;
    int detail;
    int detail_pid;
    int sigmenu;
    int sig_sel;
    int sig_pid;
    int *collapsed;
    int collapsed_count;
    int collapsed_cap;
    PvRow *rows;
    int row_count;
    int row_cap;
    int msg_pending;
    char msg[96];
    int box_x, box_y, box_w, box_h;
    int list_y;
    int list_h;
    int head_y;
    int col_start[SORT_COUNT];
    int col_end[SORT_COUNT];
} ProcView;

void pv_init(ProcView *pv);
void pv_rebuild(ProcView *pv, const ProcList *pl);
void pv_draw(ProcView *pv, const ProcList *pl, int x, int y, int w, int h, int cores);
void pv_draw_overlays(ProcView *pv, const ProcList *pl, int cols, int rows, long uptime);
int pv_key(ProcView *pv, const ProcList *pl, const TermKey *key);
int pv_overlay_active(const ProcView *pv);

#endif
