#ifndef TERM_H
#define TERM_H

#include <stddef.h>

enum {
    KEY_NONE = 0,
    KEY_TAB = 9,
    KEY_ENTER = 10,
    KEY_ESC = 27,
    KEY_BACKSPACE = 127,
    KEY_UP = 1001,
    KEY_DOWN,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_PGUP,
    KEY_PGDN,
    KEY_HOME,
    KEY_END,
    KEY_DELETE,
    KEY_F1,
    KEY_WHEEL_UP,
    KEY_WHEEL_DOWN,
    KEY_CLICK
};

typedef struct {
    int code;
    int x;
    int y;
} TermKey;

void term_enter(void);
void term_leave(void);
void term_size(int *cols, int *rows);
int term_wait(int timeout_ms);
int term_next_key(TermKey *key);
int term_quit_requested(void);
int term_take_resize(void);
void term_present(const char *data, size_t len, int clear_all);

#endif
