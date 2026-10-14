#ifndef DISK_INFO_H
#define DISK_INFO_H

#define MAX_DISKS 16

typedef struct {
    char device[96];
    char mount[192];
    char fstype[32];
    unsigned long long total;
    unsigned long long used;
    unsigned long long avail;
    double read_rate;
    double write_rate;
    int has_io;
} DiskInfo;

int fetch_disks(DiskInfo *out, int max, double dt);

#endif
