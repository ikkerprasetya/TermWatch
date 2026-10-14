#include "history.h"

void history_push(History *h, double value) {
    h->v[h->head] = value;
    h->head = (h->head + 1) % HISTORY_SIZE;
    if (h->count < HISTORY_SIZE) h->count++;
}

double history_at(const History *h, int back) {
    int idx = (h->head - 1 - back) % HISTORY_SIZE;
    if (idx < 0) idx += HISTORY_SIZE;
    return h->v[idx];
}

double history_peak(const History *h, int samples) {
    double peak = 0.0;
    if (samples > h->count) samples = h->count;
    for (int i = 0; i < samples; i++) {
        double v = history_at(h, i);
        if (v > peak) peak = v;
    }
    return peak;
}
