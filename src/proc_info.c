#include "proc_info.h"
#include <dirent.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define USER_CACHE 128

typedef struct {
    unsigned int uid;
    char name[16];
} UserEntry;

static UserEntry users[USER_CACHE];
static int user_count = 0;

static void lookup_user(unsigned int uid, char *out, size_t size) {
    for (int i = 0; i < user_count; i++) {
        if (users[i].uid == uid) {
            snprintf(out, size, "%s", users[i].name);
            return;
        }
    }
    char name[16];
    struct passwd *pw = getpwuid(uid);
    if (pw) snprintf(name, sizeof(name), "%s", pw->pw_name);
    else snprintf(name, sizeof(name), "%u", uid);
    if (user_count < USER_CACHE) {
        users[user_count].uid = uid;
        snprintf(users[user_count].name, sizeof(users[user_count].name), "%s", name);
        user_count++;
    }
    snprintf(out, size, "%s", name);
}

static int read_stat(int pid, ProcInfo *p) {
    char path[48];
    char buf[1024];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    struct stat st;
    int have_stat = fstat(fd, &st) == 0;
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) return -1;
    buf[n] = '\0';

    char *open_paren = strchr(buf, '(');
    char *close_paren = strrchr(buf, ')');
    if (!open_paren || !close_paren || close_paren < open_paren) return -1;
    size_t len = (size_t)(close_paren - open_paren - 1);
    if (len >= sizeof(p->comm)) len = sizeof(p->comm) - 1;
    memcpy(p->comm, open_paren + 1, len);
    p->comm[len] = '\0';

    char state;
    int ppid;
    unsigned long long utime, stime, start, vsize;
    long nice, threads, rss;
    int parsed = sscanf(close_paren + 2,
                        "%c %d %*d %*d %*d %*d %*u %*u %*u %*u %*u %llu %llu %*d %*d %*d %ld %ld %*d %llu %llu %ld",
                        &state, &ppid, &utime, &stime, &nice, &threads, &start, &vsize, &rss);
    if (parsed != 9) return -1;

    p->state = state;
    p->ppid = ppid;
    p->ticks = utime + stime;
    p->nice = (int)nice;
    p->threads = (int)threads;
    p->start = start;
    p->vsize = vsize;
    p->rss = rss > 0 ? (unsigned long long)rss : 0;
    p->uid = have_stat ? (unsigned int)st.st_uid : 0;
    return 0;
}

static char *read_cmdline(int pid, const char *comm) {
    char path[48];
    char buf[512];
    snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
    ssize_t n = 0;
    int fd = open(path, O_RDONLY);
    if (fd >= 0) {
        n = read(fd, buf, sizeof(buf) - 1);
        close(fd);
    }
    if (n > 0) {
        buf[n] = '\0';
        for (ssize_t i = 0; i < n; i++) {
            if ((unsigned char)buf[i] < 32) buf[i] = ' ';
        }
        while (n > 0 && buf[n - 1] == ' ') buf[--n] = '\0';
    }
    if (n <= 0) {
        char tmp[48];
        snprintf(tmp, sizeof(tmp), "[%s]", comm);
        return strdup(tmp);
    }
    return strdup(buf);
}

static int cmp_pid(const void *a, const void *b) {
    const ProcInfo *x = a;
    const ProcInfo *y = b;
    return (x->pid > y->pid) - (x->pid < y->pid);
}

void proc_update(ProcList *list, unsigned long long cpu_delta) {
    static long page_size = 0;
    if (!page_size) page_size = sysconf(_SC_PAGESIZE);

    DIR *dir = opendir("/proc");
    if (!dir) return;

    int n = 0;
    struct dirent *ent;
    while ((ent = readdir(dir))) {
        if (ent->d_name[0] < '1' || ent->d_name[0] > '9') continue;
        if (n >= list->scratch_cap) {
            int cap = list->scratch_cap ? list->scratch_cap * 2 : 256;
            ProcInfo *grown = realloc(list->scratch, (size_t)cap * sizeof(ProcInfo));
            if (!grown) break;
            list->scratch = grown;
            list->scratch_cap = cap;
        }
        ProcInfo *p = &list->scratch[n];
        memset(p, 0, sizeof(*p));
        p->pid = atoi(ent->d_name);
        if (read_stat(p->pid, p) != 0) continue;
        p->rss *= (unsigned long long)page_size;
        lookup_user(p->uid, p->user, sizeof(p->user));
        n++;
    }
    closedir(dir);

    qsort(list->scratch, (size_t)n, sizeof(ProcInfo), cmp_pid);

    int j = 0;
    int running = 0;
    int threads = 0;
    for (int k = 0; k < n; k++) {
        ProcInfo *p = &list->scratch[k];
        while (j < list->count && list->items[j].pid < p->pid) j++;
        ProcInfo *prev = (j < list->count && list->items[j].pid == p->pid && list->items[j].start == p->start)
                             ? &list->items[j]
                             : NULL;
        if (prev) {
            if (cpu_delta > 0 && p->ticks >= prev->ticks) {
                p->cpu = (double)(p->ticks - prev->ticks) / (double)cpu_delta * 100.0;
            }
            p->cmd = prev->cmd;
            prev->cmd = NULL;
        } else {
            p->cmd = read_cmdline(p->pid, p->comm);
        }
        if (p->state == 'R') running++;
        threads += p->threads;
    }

    for (int i = 0; i < list->count; i++) free(list->items[i].cmd);

    ProcInfo *tmp = list->items;
    int tmp_cap = list->cap;
    list->items = list->scratch;
    list->cap = list->scratch_cap;
    list->scratch = tmp;
    list->scratch_cap = tmp_cap;
    list->count = n;
    list->running = running;
    list->total_threads = threads;
}
