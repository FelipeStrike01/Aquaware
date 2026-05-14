#include "../../include/core/validation.h"
#include "../helpers.h"
#include <ntddk.h>

BOOLEAN validate_user_range(const void* address, SIZE_T size, KPROCESSOR_MODE mode) {
    if (!address || !size) return FALSE;
    if (mode != UserMode) return TRUE;
    if (size > MAXULONG) return FALSE;

    ULONG_PTR start = (ULONG_PTR)address;
    ULONG_PTR end = start + size;
    if (end < start) return FALSE;
    if (start >= (ULONG_PTR)MmUserProbeAddress) return FALSE;
    if (end > (ULONG_PTR)MmUserProbeAddress) return FALSE;

    __try {
        ProbeForRead(const_cast<volatile void*>(address), (ULONG)size, 1);
        return TRUE;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return FALSE;
    }
}

BOOLEAN validate_physical_address(ULONG64 address) {
    if (address < 0x1000) return FALSE;
    if (address > 0xFFFFFFFFFF) return FALSE;
    return TRUE;
}

BOOLEAN validate_self_ref_map(ULONG64 address) {
    if (address & SIGN_EXTEND_MASK) {
        if ((address & SIGN_EXTEND_MASK) == SIGN_EXTEND_MASK) {
            address &= ~SIGN_EXTEND_MASK;
        } else {
            return FALSE;
        }
    }
    if (address >= 0x7FFFFFFFFFFF) return FALSE;
    return TRUE;
}