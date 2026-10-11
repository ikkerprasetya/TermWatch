#ifndef TERM_H
#define TERM_H

#include <stddef.h>

void term_enter(void);
void term_leave(void);
void term_size(int *cols, int *rows);
int term_wait(int timeout_ms);
int term_quit_requested(void);
int term_take_resize(void);
void term_present(const char *data, size_t len, int clear_all);

#endif
