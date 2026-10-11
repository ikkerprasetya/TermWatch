#ifndef UI_H
#define UI_H

#include "system_info.h"
#include "cpu_info.h"
#include "mem_info.h"

void ui_push_cpu_sample(double percent);
void ui_render(const SystemInfo *sys, const CpuInfo *cpu, const MemoryInfo *mem,
               int cols, int clear_all);

#endif
