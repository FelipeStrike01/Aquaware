#pragma once
#include <ntddk.h>

void init_logging();
void cleanup_logging();
void write_log_entry(ULONG level, ULONG64 data1, ULONG64 data2);