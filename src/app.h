#ifndef APP_H
#define APP_H

#include "system_info.h"
#include "cpu_info.h"
#include "mem_info.h"
#include "disk_info.h"
#include "net_info.h"
#include "proc_info.h"
#include "history.h"

typedef struct {
    SystemInfo sys;
    CpuInfo cpu;
    MemoryInfo mem;
    DiskInfo disks[MAX_DISKS];
    int disk_count;
    NetInfo net;
    ProcList procs;
    History cpu_hist;
} AppData;

#endif
