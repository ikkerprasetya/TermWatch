#include "net_info.h"
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

static NetIface *find_or_add(NetInfo *net, const char *name) {
    for (int i = 0; i < net->count; i++) {
        if (strcmp(net->ifaces[i].name, name) == 0) return &net->ifaces[i];
    }
    if (net->count >= MAX_IFACES) return NULL;
    NetIface *n = &net->ifaces[net->count++];
    memset(n, 0, sizeof(*n));
    snprintf(n->name, sizeof(n->name), "%.*s", (int)sizeof(n->name) - 1, name);
    return n;
}

static void update_addresses(NetInfo *net) {
    struct ifaddrs *list;
    if (getifaddrs(&list) != 0) return;
    for (struct ifaddrs *a = list; a; a = a->ifa_next) {
        if (!a->ifa_addr || a->ifa_addr->sa_family != AF_INET) continue;
        NetIface *n = NULL;
        for (int i = 0; i < net->count; i++) {
            if (strcmp(net->ifaces[i].name, a->ifa_name) == 0) {
                n = &net->ifaces[i];
                break;
            }
        }
        if (!n) continue;
        const struct sockaddr_in *sin = (const struct sockaddr_in *)(const void *)a->ifa_addr;
        inet_ntop(AF_INET, &sin->sin_addr, n->ip, sizeof(n->ip));
    }
    freeifaddrs(list);
}

void net_update(NetInfo *net, double dt) {
    FILE *file = fopen("/proc/net/dev", "r");
    if (!file) return;

    char line[512];
    int row = 0;
    while (fgets(line, sizeof(line), file)) {
        if (row++ < 2) continue;
        char *colon = strchr(line, ':');
        if (!colon) continue;
        *colon = '\0';
        char *name = line;
        while (*name == ' ') name++;

        unsigned long long rx, tx;
        if (sscanf(colon + 1, "%llu %*u %*u %*u %*u %*u %*u %*u %llu", &rx, &tx) != 2) continue;

        NetIface *n = find_or_add(net, name);
        if (!n) continue;

        if (n->seen && dt > 0.0) {
            n->rx_rate = rx >= n->rx ? (double)(rx - n->rx) / dt : 0.0;
            n->tx_rate = tx >= n->tx ? (double)(tx - n->tx) / dt : 0.0;
            if (n->rx_rate > n->rx_top) n->rx_top = n->rx_rate;
            if (n->tx_rate > n->tx_top) n->tx_top = n->tx_rate;
            history_push(&n->rx_hist, n->rx_rate);
            history_push(&n->tx_hist, n->tx_rate);
        } else {
            n->rx_base = rx;
            n->tx_base = tx;
        }
        n->rx = rx;
        n->tx = tx;
        n->seen = 1;
    }
    fclose(file);
    update_addresses(net);
}

void net_reset_totals(NetInfo *net, int index) {
    if (index < 0 || index >= net->count) return;
    NetIface *n = &net->ifaces[index];
    n->rx_base = n->rx;
    n->tx_base = n->tx;
    n->rx_top = 0.0;
    n->tx_top = 0.0;
}
