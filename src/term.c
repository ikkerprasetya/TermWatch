#include "term.h"
#include <poll.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#define INPUT_CAP 512

static const char ENTER_SEQ[] = "\033[?1049h\033[?25l\033[?1000h\033[?1006h\033[2J";
static const char LEAVE_SEQ[] = "\033[0m\033[?1006l\033[?1000l\033[?25h\033[?1049l";

static struct termios saved_mode;
static int raw_active = 0;
static int mode_saved = 0;
static volatile sig_atomic_t quit_flag = 0;
static volatile sig_atomic_t resize_flag = 1;

static unsigned char input[INPUT_CAP];
static int input_len = 0;
static int input_pos = 0;

static void write_all(const char *data, size_t len) {
    while (len > 0) {
        ssize_t n = write(STDOUT_FILENO, data, len);
        if (n < 0) return;
        data += n;
        len -= (size_t)n;
    }
}

static void on_quit(int sig) {
    (void)sig;
    quit_flag = 1;
}

static void on_resize(int sig) {
    (void)sig;
    resize_flag = 1;
}

static void restore_screen(void) {
    write_all(LEAVE_SEQ, sizeof(LEAVE_SEQ) - 1);
    if (mode_saved) tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_mode);
}

static void apply_raw(void) {
    if (mode_saved) {
        struct termios raw = saved_mode;
        raw.c_lflag &= ~(tcflag_t)(ICANON | ECHO);
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    }
    write_all(ENTER_SEQ, sizeof(ENTER_SEQ) - 1);
}

static void on_stop(int sig) {
    (void)sig;
    restore_screen();
    signal(SIGTSTP, SIG_DFL);
    raise(SIGTSTP);
}

static void on_cont(int sig) {
    (void)sig;
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = on_stop;
    sigaction(SIGTSTP, &sa, NULL);
    apply_raw();
    resize_flag = 1;
}

void term_leave(void) {
    if (!raw_active) return;
    raw_active = 0;
    restore_screen();
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

    sa.sa_handler = on_stop;
    sigaction(SIGTSTP, &sa, NULL);

    sa.sa_handler = on_cont;
    sigaction(SIGCONT, &sa, NULL);

    if (tcgetattr(STDIN_FILENO, &saved_mode) == 0) mode_saved = 1;

    raw_active = 1;
    atexit(term_leave);
    apply_raw();
}

void term_size(int *cols, int *rows) {
    struct winsize ws;
    *cols = 80;
    *rows = 24;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0 && ws.ws_row > 0) {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
    }
}

int term_wait(int timeout_ms) {
    if (input_pos < input_len) return 1;
    input_pos = 0;
    input_len = 0;

    struct pollfd pfd = { STDIN_FILENO, POLLIN, 0 };
    if (timeout_ms < 0) timeout_ms = 0;
    if (poll(&pfd, 1, timeout_ms) > 0 && (pfd.revents & POLLIN)) {
        ssize_t n = read(STDIN_FILENO, input, sizeof(input));
        if (n > 0) input_len = (int)n;
    }
    return input_len > 0;
}

static int parse_number(const unsigned char *p, int n, int *pos, int *value) {
    int v = 0;
    int any = 0;
    while (*pos < n && p[*pos] >= '0' && p[*pos] <= '9') {
        v = v * 10 + (p[*pos] - '0');
        (*pos)++;
        any = 1;
    }
    *value = v;
    return any;
}

static int parse_mouse(const unsigned char *p, int n, int *used, TermKey *key) {
    int pos = 3;
    int b, x, y;
    if (!parse_number(p, n, &pos, &b) || pos >= n || p[pos] != ';') return 0;
    pos++;
    if (!parse_number(p, n, &pos, &x) || pos >= n || p[pos] != ';') return 0;
    pos++;
    if (!parse_number(p, n, &pos, &y) || pos >= n) return 0;
    int final = p[pos];
    pos++;
    *used = pos;
    key->code = KEY_NONE;
    key->x = x - 1;
    key->y = y - 1;
    if (final == 'M') {
        if (b & 64) {
            if ((b & 3) == 0) key->code = KEY_WHEEL_UP;
            else if ((b & 3) == 1) key->code = KEY_WHEEL_DOWN;
        } else if ((b & 3) == 0 && !(b & 32)) {
            key->code = KEY_CLICK;
        }
    }
    return 1;
}

static int parse_escape(const unsigned char *p, int n, int *used, TermKey *key) {
    *used = 1;
    key->code = KEY_ESC;
    if (n < 2) return 1;

    if (p[1] == 'O' && n >= 3) {
        *used = 3;
        switch (p[2]) {
        case 'A': key->code = KEY_UP; break;
        case 'B': key->code = KEY_DOWN; break;
        case 'C': key->code = KEY_RIGHT; break;
        case 'D': key->code = KEY_LEFT; break;
        case 'H': key->code = KEY_HOME; break;
        case 'F': key->code = KEY_END; break;
        case 'P': key->code = KEY_F1; break;
        default: key->code = KEY_NONE; break;
        }
        return 1;
    }

    if (p[1] != '[') return 1;

    if (n >= 3 && p[2] == '<') {
        if (parse_mouse(p, n, used, key)) return 1;
        *used = n;
        key->code = KEY_NONE;
        return 1;
    }

    int pos = 2;
    int num = 0;
    parse_number(p, n, &pos, &num);
    while (pos < n && (p[pos] == ';' || (p[pos] >= '0' && p[pos] <= '9'))) pos++;
    if (pos >= n) {
        *used = n;
        key->code = KEY_NONE;
        return 1;
    }
    int final = p[pos];
    *used = pos + 1;
    key->code = KEY_NONE;
    switch (final) {
    case 'A': key->code = KEY_UP; break;
    case 'B': key->code = KEY_DOWN; break;
    case 'C': key->code = KEY_RIGHT; break;
    case 'D': key->code = KEY_LEFT; break;
    case 'H': key->code = KEY_HOME; break;
    case 'F': key->code = KEY_END; break;
    case '~':
        switch (num) {
        case 1: case 7: key->code = KEY_HOME; break;
        case 4: case 8: key->code = KEY_END; break;
        case 3: key->code = KEY_DELETE; break;
        case 5: key->code = KEY_PGUP; break;
        case 6: key->code = KEY_PGDN; break;
        case 11: key->code = KEY_F1; break;
        default: break;
        }
        break;
    default:
        break;
    }
    return 1;
}

int term_next_key(TermKey *key) {
    while (input_pos < input_len) {
        const unsigned char *p = input + input_pos;
        int n = input_len - input_pos;
        int used = 1;
        key->code = KEY_NONE;
        key->x = 0;
        key->y = 0;

        if (p[0] == 0x1b) {
            parse_escape(p, n, &used, key);
        } else if (p[0] == '\r' || p[0] == '\n') {
            key->code = KEY_ENTER;
        } else if (p[0] == 127 || p[0] == 8) {
            key->code = KEY_BACKSPACE;
        } else if (p[0] == '\t') {
            key->code = KEY_TAB;
        } else if (p[0] >= 32 && p[0] < 127) {
            key->code = p[0];
        }

        input_pos += used;
        if (key->code != KEY_NONE) return 1;
    }
    return 0;
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
    write_all("\033[?2026l", 8);
}
