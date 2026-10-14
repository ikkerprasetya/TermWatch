#include "system_info.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/sysinfo.h>

static void read_pretty_name(char *out, size_t size) {
    FILE *file = fopen("/etc/os-release", "r");
    if (!file) return;
    char line[256];
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "PRETTY_NAME=", 12) == 0) {
            char *value = line + 12;
            value[strcspn(value, "\r\n")] = '\0';
            size_t len = strlen(value);
            if (len >= 2 && value[0] == '"' && value[len - 1] == '"') {
                value[len - 1] = '\0';
                value++;
            }
            if (value[0] != '\0') snprintf(out, size, "%.*s", (int)size - 1, value);
            break;
        }
    }
    fclose(file);
}

int fetch_system_info(SystemInfo *info) {
    if (!info) return -1;
    static char pretty[128];
    static int pretty_loaded = 0;

    struct utsname uname_data;
    if (uname(&uname_data) == 0) {
        snprintf(info->os_name, sizeof(info->os_name), "%s", uname_data.sysname);
        snprintf(info->kernel_version, sizeof(info->kernel_version), "%s", uname_data.release);
    } else {
        snprintf(info->os_name, sizeof(info->os_name), "Unknown");
        snprintf(info->kernel_version, sizeof(info->kernel_version), "Unknown");
    }

    if (!pretty_loaded) {
        read_pretty_name(pretty, sizeof(pretty));
        pretty_loaded = 1;
    }
    if (pretty[0] != '\0') snprintf(info->os_name, sizeof(info->os_name), "%s", pretty);

    if (gethostname(info->hostname, sizeof(info->hostname)) != 0) {
        snprintf(info->hostname, sizeof(info->hostname), "Unknown");
    }
    info->hostname[sizeof(info->hostname) - 1] = '\0';

    struct sysinfo s_info;
    if (sysinfo(&s_info) == 0) {
        info->uptime_seconds = s_info.uptime;
    } else {
        info->uptime_seconds = 0;
    }

    if (getloadavg(info->load, 3) != 3) {
        info->load[0] = info->load[1] = info->load[2] = 0.0;
    }

    return 0;
}
