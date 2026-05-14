#pragma once
#include <ntddk.h>
#include "../common/driver_types.h"

BOOLEAN validate_user_range(const void* address, SIZE_T size, KPROCESSOR_MODE mode);
BOOLEAN validate_physical_address(ULONG64 address);
BOOLEAN validate_self_ref_map(ULONG64 address);