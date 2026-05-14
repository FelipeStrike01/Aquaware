#include "../../include/core/state.h"
#include "../helpers.h"
#include <ntddk.h>

#pragma comment(lib, "ntoskrnl.lib")

// Forward declaration
extern "C" NTSTATUS PsLookupProcessByProcessId(HANDLE ProcessId, PEPROCESS* Process);

static PROCESS_CONTEXT g_active_context = { nullptr, 0, 0, 0 };
static FAST_MUTEX g_state_lock;

void init_state() {
    ExInitializeFastMutex(&g_state_lock);
    RtlZeroMemory(&g_active_context, sizeof(PROCESS_CONTEXT));
}

void cleanup_state() {
    ExAcquireFastMutex(&g_state_lock);
    if (g_active_context.eprocess) {
        ObDereferenceObject(g_active_context.eprocess);
    }
    RtlZeroMemory(&g_active_context, sizeof(PROCESS_CONTEXT));
    ExReleaseFastMutex(&g_state_lock);
}

ULONG64 get_process_directory_base(PEPROCESS process) {
    if (!process) return 0;
    ULONG64 dirbase = *(ULONG64*)((ULONG64)process + 0x28);
    return dirbase & ~0xF;
}

NTSTATUS set_active_process(HANDLE pid) {
    ExAcquireFastMutex(&g_state_lock);

    if (g_active_context.eprocess) {
        ObDereferenceObject(g_active_context.eprocess);
        RtlZeroMemory(&g_active_context, sizeof(PROCESS_CONTEXT));
    }

    PEPROCESS process = nullptr;
    NTSTATUS status = PsLookupProcessByProcessId(pid, &process);

    if (NT_SUCCESS(status)) {
        g_active_context.eprocess = process;
        g_active_context.pid = pid;
        g_active_context.directory_base = get_process_directory_base(process);
        g_active_context.is_valid = 1;
    }

    ExReleaseFastMutex(&g_state_lock);
    return status;
}

PROCESS_CONTEXT* get_active_process() {
    return (g_active_context.is_valid) ? &g_active_context : nullptr;
}