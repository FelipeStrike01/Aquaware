#pragma once
#include <ntddk.h>

struct PROCESS_CONTEXT {
    PEPROCESS eprocess;
    HANDLE pid;
    ULONG64 directory_base;
    volatile LONG is_valid;
};

void init_state();
void cleanup_state();
NTSTATUS set_active_process(HANDLE pid);
PROCESS_CONTEXT* get_active_process();
ULONG64 get_process_directory_base(PEPROCESS process);