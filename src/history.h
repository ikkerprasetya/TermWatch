#ifndef HISTORY_H
#define HISTORY_H

#define HISTORY_SIZE 1024

typedef struct {
    double v[HISTORY_SIZE];
    int head;
    int count;
} History;

void history_push(History *h, double value);
double history_at(const History *h, int back);
double history_peak(const History *h, int samples);

#endif
