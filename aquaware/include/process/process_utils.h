#pragma once
#include <ntddk.h>
#include "../common/driver_types.h"

NTSTATUS read_physical_memory(ULONG64 physical_address, void* buffer, ULONG64 size);
NTSTATUS write_physical_memory(ULONG64 physical_address, void* buffer, ULONG64 size);
ULONG64 translate_linear_address(ULONG64 directory_base, ULONG64 virtual_address);
NTSTATUS read_virtual_memory(ULONG64 directory_base, ULONG64 virtual_address, void* buffer, ULONG64 size);
NTSTATUS write_virtual_memory(ULONG64 directory_base, ULONG64 virtual_address, void* buffer, ULONG64 size);
ULONG64 get_kernel_export(const wchar_t* module_name, const char* function_name);
ULONG64 resolve_ntoskrnl_base();