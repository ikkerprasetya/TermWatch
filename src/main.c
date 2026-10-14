#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "app.h"
#include "term.h"
#include "ui.h"

#define FIRST_SAMPLE_MS 300

static AppData data;
static UiState ui;

static long long now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static void sample(AppData *d, double dt) {
    fetch_system_info(&d->sys);
    fetch_mem_info(&d->mem);
    cpu_update(&d->cpu);
    history_push(&d->cpu_hist, d->cpu.usage_percent);
    d->disk_count = fetch_disks(d->disks, MAX_DISKS, dt);
    net_update(&d->net, dt);
    proc_update(&d->procs, d->cpu.delta_total);
}

int main(void) {
    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
        fprintf(stderr, "termwatch: an interactive terminal is required\n");
        return 1;
    }

    memset(&data, 0, sizeof(data));
    fetch_cpu_static_info(&data.cpu);
    ui_init(&ui);

    long long last = now_ms();
    sample(&data, 1.0);
    ui_pick_interface(&ui, &data);

    term_enter();

    long long next_sample = now_ms() + FIRST_SAMPLE_MS;
    int running = 1;

    while (running && !term_quit_requested()) {
        int cols, rows;
        term_size(&cols, &rows);
        ui_render(&ui, &data, cols, rows, term_take_resize(), now_ms());

        long long wait = next_sample - now_ms();
        term_wait(wait > 0 ? (int)wait : 0);

        TermKey key;
        while (term_next_key(&key)) {
            int result = ui_key(&ui, &data, &key, now_ms());
            if (result & UI_QUIT) {
                running = 0;
                break;
            }
            if (result & UI_RESAMPLE) next_sample = now_ms();
        }

        long long now = now_ms();
        if (running && now >= next_sample) {
            sample(&data, (double)(now - last) / 1000.0);
            last = now;
            next_sample = now + ui.interval_ms;
        }
    }

    return 0;
}
