#ifndef SYSTEM_INFO_H
#define SYSTEM_INFO_H

typedef struct {
    char os_name[128];
    char kernel_version[128];
    char hostname[128];
    long uptime_seconds;
    double load[3];
} SystemInfo;

int fetch_system_info(SystemInfo *info);

#endif
