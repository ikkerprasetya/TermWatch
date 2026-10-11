<<<<<<< HEAD
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
=======
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include "system_info.h"
#include "cpu_info.h"
#include "mem_info.h"

static volatile int keep_running = 1;

void handle_sigint(int sig) {
    (void)sig;
    keep_running = 0;
}

int main(void) {
    signal(SIGINT, handle_sigint);

    SystemInfo sys;
    CpuInfo cpu;
    MemoryInfo mem;

    fetch_cpu_static_info(&cpu);
    calculate_cpu_usage();

    // Hide terminal cursor
    printf("\033[?25l");

    while (keep_running) {
        fetch_system_info(&sys);
        cpu.usage_percent = calculate_cpu_usage();
        fetch_mem_info(&mem);

        // Native Linux terminal clear
        if (system("clear") != 0) {
            printf("\033[H\033[J");
        }

        print_system_info(&sys);
        print_cpu_info(&cpu);
        print_mem_info(&mem);

        printf("\033[90mPress Ctrl+C to exit. Refresh rate: 1s\033[0m\n");
        fflush(stdout);

        sleep(1);
    }

    // Restore cursor visibility
    printf("\033[?25h\nExiting cleanly...\n");
    return 0;
}
>>>>>>> 6ec40683499a86f91f1e5cd6917dca80c8d1fee0
