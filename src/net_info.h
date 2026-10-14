#ifndef NET_INFO_H
#define NET_INFO_H

#include "history.h"

#define MAX_IFACES 16

typedef struct {
    char name[32];
    char ip[48];
    unsigned long long rx;
    unsigned long long tx;
    unsigned long long rx_base;
    unsigned long long tx_base;
    double rx_rate;
    double tx_rate;
    double rx_top;
    double tx_top;
    int seen;
    History rx_hist;
    History tx_hist;
} NetIface;

typedef struct {
    NetIface ifaces[MAX_IFACES];
    int count;
} NetInfo;

void net_update(NetInfo *net, double dt);
void net_reset_totals(NetInfo *net, int index);

#endif
