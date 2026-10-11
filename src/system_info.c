<<<<<<< HEAD
#include "system_info.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/sysinfo.h>

int fetch_system_info(SystemInfo *info) {
    if (!info) return -1;

    struct utsname uname_data;
    if (uname(&uname_data) == 0) {
        snprintf(info->os_name, sizeof(info->os_name), "%s", uname_data.sysname);
        snprintf(info->kernel_version, sizeof(info->kernel_version), "%s", uname_data.release);
    } else {
        snprintf(info->os_name, sizeof(info->os_name), "Unknown");
        snprintf(info->kernel_version, sizeof(info->kernel_version), "Unknown");
    }

    if (gethostname(info->hostname, sizeof(info->hostname)) != 0) {
        snprintf(info->hostname, sizeof(info->hostname), "Unknown");
    }

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
=======
#define _DEFAULT_SOURCE
#include "system_info.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/sysinfo.h>

int fetch_system_info(SystemInfo *info) {
    if (!info) return -1;

    struct utsname uname_data;
    if (uname(&uname_data) == 0) {
        snprintf(info->os_name, sizeof(info->os_name), "%s", uname_data.sysname);
        snprintf(info->kernel_version, sizeof(info->kernel_version), "%s", uname_data.release);
    } else {
        snprintf(info->os_name, sizeof(info->os_name), "Unknown");
        snprintf(info->kernel_version, sizeof(info->kernel_version), "Unknown");
    }

    if (gethostname(info->hostname, sizeof(info->hostname)) != 0) {
        snprintf(info->hostname, sizeof(info->hostname), "Unknown");
    }

    struct sysinfo s_info;
    if (sysinfo(&s_info) == 0) {
        info->uptime_seconds = s_info.uptime;
    } else {
        info->uptime_seconds = 0;
    }

    return 0;
}

void print_system_info(const SystemInfo *info) {
    long days = info->uptime_seconds / 86400;
    long hours = (info->uptime_seconds % 86400) / 3600;
    long mins = (info->uptime_seconds % 3600) / 60;
    long secs = info->uptime_seconds % 60;

    printf("\033[1;36m================ SYSTEM INFORMATION ================\033[0m\n");
    printf("  Hostname       : %s\n", info->hostname);
    printf("  OS             : %s\n", info->os_name);
    printf("  Kernel         : %s\n", info->kernel_version);
    printf("  Uptime         : %ldd %ldh %ldm %lds\n\n", days, hours, mins, secs);
}
>>>>>>> 6ec40683499a86f91f1e5cd6917dca80c8d1fee0
