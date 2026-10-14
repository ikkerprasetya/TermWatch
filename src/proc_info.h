#ifndef PROC_INFO_H
#define PROC_INFO_H

typedef struct {
    int pid;
    int ppid;
    char comm[32];
    char *cmd;
    char user[16];
    char state;
    int threads;
    int nice;
    unsigned int uid;
    unsigned long long rss;
    unsigned long long vsize;
    unsigned long long ticks;
    unsigned long long start;
    double cpu;
} ProcInfo;

typedef struct {
    ProcInfo *items;
    int count;
    int cap;
    ProcInfo *scratch;
    int scratch_cap;
    int running;
    int total_threads;
} ProcList;

void proc_update(ProcList *list, unsigned long long cpu_delta);

#endif
