#include <time.h>
#include "system_info.h"
#include "cpu_info.h"
#include "mem_info.h"
#include "term.h"
#include "ui.h"

#define SAMPLE_INTERVAL_MS 1000
#define FIRST_SAMPLE_MS 300

static long long now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

int main(void) {
    SystemInfo sys;
    CpuInfo cpu;
    MemoryInfo mem;

    fetch_cpu_static_info(&cpu);
    cpu.usage_percent = 0.0;
    calculate_cpu_usage();
    fetch_system_info(&sys);
    fetch_mem_info(&mem);

    term_enter();

    long long next_sample = now_ms() + FIRST_SAMPLE_MS;

    while (!term_quit_requested()) {
        int cols, rows;
        term_size(&cols, &rows);
        ui_render(&sys, &cpu, &mem, cols, term_take_resize());

        int key = term_wait((int)(next_sample - now_ms()));
        if (key == 'q' || key == 'Q') break;

        if (now_ms() >= next_sample) {
            fetch_system_info(&sys);
            fetch_mem_info(&mem);
            cpu.usage_percent = calculate_cpu_usage();
            ui_push_cpu_sample(cpu.usage_percent);
            next_sample = now_ms() + SAMPLE_INTERVAL_MS;
        }
    }

    return 0;
}
