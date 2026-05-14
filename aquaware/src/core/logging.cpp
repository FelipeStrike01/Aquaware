#include "../../include/core/logging.h"
#include "../helpers.h"
#include <ntddk.h>

static volatile LONG g_log_initialized = 0;
static volatile LONG g_log_index = 0;
static ULONG64 g_log_buffer[1024];

void init_logging() {
    RtlZeroMemory((PVOID)g_log_buffer, sizeof(g_log_buffer));
    InterlockedExchange(&g_log_initialized, 1);
}

void cleanup_logging() {
    InterlockedExchange(&g_log_initialized, 0);
}

void write_log_entry(ULONG level, ULONG64 data1, ULONG64 data2) {
    if (!InterlockedCompareExchange(&g_log_initialized, 1, 1)) {
        return;
    }
    LONG index = InterlockedIncrement(&g_log_index) & 0x3FF;
    g_log_buffer[index] = ((ULONG64)level << 56) | (data1 & 0x00FFFFFFFFFFFFFF);
    g_log_buffer[(index + 1) & 0x3FF] = data2;
}