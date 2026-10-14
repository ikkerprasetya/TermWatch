#include "disk_info.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/statvfs.h>

typedef struct {
    char name[48];
    unsigned long long read_sectors;
    unsigned long long write_sectors;
    int used;
} IoState;

static IoState io_prev[MAX_DISKS];

static int is_remote_fs(const char *type) {
    return strcmp(type, "zfs") == 0 || strcmp(type, "nfs") == 0 || strcmp(type, "nfs4") == 0 ||
           strcmp(type, "cifs") == 0;
}

static int skip_mount(const char *mount) {
    return strncmp(mount, "/snap", 5) == 0 || strncmp(mount, "/var/lib/docker", 15) == 0 ||
           strncmp(mount, "/var/lib/kubelet", 16) == 0 || strncmp(mount, "/run/", 5) == 0;
}

static int read_io_counters(const char *device, unsigned long long *rd, unsigned long long *wr) {
    char resolved[PATH_MAX];
    const char *path = device;
    if (realpath(device, resolved)) path = resolved;
    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;

    FILE *file = fopen("/proc/diskstats", "r");
    if (!file) return -1;
    char line[256];
    int found = -1;
    while (fgets(line, sizeof(line), file)) {
        char name[64];
        unsigned long long r, w;
        if (sscanf(line, "%*u %*u %63s %*u %*u %llu %*u %*u %*u %llu", name, &r, &w) == 3 &&
            strcmp(name, base) == 0) {
            *rd = r;
            *wr = w;
            found = 0;
            break;
        }
    }
    fclose(file);
    return found;
}

static IoState *io_slot(const char *name) {
    IoState *free_slot = NULL;
    for (int i = 0; i < MAX_DISKS; i++) {
        if (io_prev[i].used && strcmp(io_prev[i].name, name) == 0) return &io_prev[i];
        if (!io_prev[i].used && !free_slot) free_slot = &io_prev[i];
    }
    if (free_slot) {
        memset(free_slot, 0, sizeof(*free_slot));
        snprintf(free_slot->name, sizeof(free_slot->name), "%.*s", (int)sizeof(free_slot->name) - 1, name);
    }
    return free_slot;
}

int fetch_disks(DiskInfo *out, int max, double dt) {
    FILE *file = fopen("/proc/mounts", "r");
    if (!file) return 0;

    int count = 0;
    char line[512];
    while (count < max && fgets(line, sizeof(line), file)) {
        char device[96], mount[192], type[32];
        if (sscanf(line, "%95s %191s %31s", device, mount, type) != 3) continue;

        int is_root = strcmp(mount, "/") == 0;
        int is_block = strncmp(device, "/dev/", 5) == 0 && strncmp(device, "/dev/loop", 9) != 0 &&
                       strncmp(device, "/dev/ram", 8) != 0;
        if (!is_block && !is_root && !is_remote_fs(type)) continue;
        if (skip_mount(mount)) continue;

        int duplicate = 0;
        for (int i = 0; i < count; i++) {
            if (is_block ? strcmp(out[i].device, device) == 0 : strcmp(out[i].mount, mount) == 0) {
                duplicate = 1;
                break;
            }
        }
        if (duplicate) continue;

        struct statvfs sv;
        if (statvfs(mount, &sv) != 0 || sv.f_blocks == 0) continue;

        DiskInfo *d = &out[count];
        memset(d, 0, sizeof(*d));
        snprintf(d->device, sizeof(d->device), "%s", device);
        snprintf(d->mount, sizeof(d->mount), "%s", mount);
        snprintf(d->fstype, sizeof(d->fstype), "%s", type);
        d->total = (unsigned long long)sv.f_blocks * sv.f_frsize;
        d->used = (unsigned long long)(sv.f_blocks - sv.f_bfree) * sv.f_frsize;
        d->avail = (unsigned long long)sv.f_bavail * sv.f_frsize;

        unsigned long long rd, wr;
        if (is_block && read_io_counters(device, &rd, &wr) == 0) {
            IoState *slot = io_slot(device);
            if (slot) {
                if (slot->used && dt > 0.0) {
                    d->has_io = 1;
                    d->read_rate = rd >= slot->read_sectors ? (double)(rd - slot->read_sectors) * 512.0 / dt : 0.0;
                    d->write_rate = wr >= slot->write_sectors ? (double)(wr - slot->write_sectors) * 512.0 / dt : 0.0;
                }
                slot->read_sectors = rd;
                slot->write_sectors = wr;
                slot->used = 1;
            }
        }
        count++;
    }
    fclose(file);
    return count;
}
