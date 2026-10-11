#include "term.h"
#include <poll.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

static struct termios saved_mode;
static int raw_active = 0;
static volatile sig_atomic_t quit_flag = 0;
static volatile sig_atomic_t resize_flag = 1;

static void on_quit(int sig) {
    (void)sig;
    quit_flag = 1;
}

static void on_resize(int sig) {
    (void)sig;
    resize_flag = 1;
}

static void write_all(const char *data, size_t len) {
    while (len > 0) {
        ssize_t n = write(STDOUT_FILENO, data, len);
        if (n < 0) return;
        data += n;
        len -= (size_t)n;
    }
}

void term_leave(void) {
    if (!raw_active) return;
    raw_active = 0;
    const char *seq = "\033[0m\033[?25h\033[?1049l";
    write_all(seq, strlen(seq));
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_mode);
}

void term_enter(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sigemptyset(&sa.sa_mask);

    sa.sa_handler = on_quit;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);

    sa.sa_handler = on_resize;
    sigaction(SIGWINCH, &sa, NULL);

    if (tcgetattr(STDIN_FILENO, &saved_mode) == 0) {
        struct termios raw = saved_mode;
        raw.c_lflag &= ~(tcflag_t)(ICANON | ECHO);
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    }

    raw_active = 1;
    atexit(term_leave);
    const char *seq = "\033[?1049h\033[?25l\033[2J";
    write_all(seq, strlen(seq));
}

void term_size(int *cols, int *rows) {
    struct winsize ws;
    *cols = 80;
    *rows = 24;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
    }
}

int term_wait(int timeout_ms) {
    struct pollfd pfd = { STDIN_FILENO, POLLIN, 0 };
    int key = 0;
    if (timeout_ms < 0) timeout_ms = 0;
    if (poll(&pfd, 1, timeout_ms) > 0 && (pfd.revents & POLLIN)) {
        char buf[32];
        ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
        if (n > 0) key = (unsigned char)buf[n - 1];
    }
    return key;
}

int term_quit_requested(void) {
    return quit_flag;
}

int term_take_resize(void) {
    int flag = resize_flag;
    resize_flag = 0;
    return flag;
}

void term_present(const char *data, size_t len, int clear_all) {
    write_all("\033[?2026h\033[H", 11);
    if (clear_all) write_all("\033[2J", 4);
    write_all(data, len);
    write_all("\033[J\033[?2026l", 11);
}
