#ifndef UI_H
#define UI_H

#include "app.h"
#include "procview.h"
#include "term.h"

#define UI_QUIT 1
#define UI_RESAMPLE 2

typedef struct {
    int interval_ms;
    int show[4];
    int net_sel;
    int help;
    char message[96];
    long long message_until;
    ProcView pv;
} UiState;

void ui_init(UiState *ui);
void ui_pick_interface(UiState *ui, const AppData *data);
void ui_render(UiState *ui, const AppData *data, int cols, int rows, int clear_all, long long now);
int ui_key(UiState *ui, AppData *data, const TermKey *key, long long now);

#endif
