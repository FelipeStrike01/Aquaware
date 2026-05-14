#include "../../include/process/process_utils.h"
#include "../../include/core/validation.h"
#include "../helpers.h"
#include <ntddk.h>
#include <ntimage.h>

extern "C" PLIST_ENTRY PsLoadedModuleList;

// LDR_DATA_TABLE_ENTRY definition
typedef struct _LDR_DATA_TABLE_ENTRY {
    LIST_ENTRY InLoadOrderLinks;
    LIST_ENTRY InMemoryOrderLinks;
    LIST_ENTRY InInitializationOrderLinks;
    PVOID DllBase;
    PVOID EntryPoint;
    ULONG SizeOfImage;
    UNICODE_STRING FullDllName;
    UNICODE_STRING BaseDllName;
} LDR_DATA_TABLE_ENTRY, * PLDR_DATA_TABLE_ENTRY;

typedef LDR_DATA_TABLE_ENTRY KLDR_DATA_TABLE_ENTRY;
typedef LDR_DATA_TABLE_ENTRY* PKLDR_DATA_TABLE_ENTRY;

static ULONG64 g_self_ref_pml4 = 0;

NTSTATUS read_physical_memory(ULONG64 physical_address, void* buffer, ULONG64 size) {
    if (!validate_physical_address(physical_address) || !buffer || !size) {
        return STATUS_INVALID_PARAMETER;
    }
    if (size > (ULONG64)MAXULONG_PTR) return STATUS_INVALID_PARAMETER;
    if (physical_address + size < physical_address) return STATUS_INVALID_PARAMETER;

    PHYSICAL_ADDRESS phys_addr;
    phys_addr.QuadPart = physical_address;

    PVOID mapped = MmMapIoSpace(phys_addr, size, MmNonCached);
    if (!mapped) {
        return STATUS_UNSUCCESSFUL;
    }

    __try {
        memcpy(buffer, mapped, (size_t)size);
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        MmUnmapIoSpace(mapped, size);
        return STATUS_UNSUCCESSFUL;
    }

    MmUnmapIoSpace(mapped, size);
    return STATUS_SUCCESS;
}

NTSTATUS write_physical_memory(ULONG64 physical_address, void* buffer, ULONG64 size) {
    if (!validate_physical_address(physical_address) || !buffer || !size) {
        return STATUS_INVALID_PARAMETER;
    }
    if (size > (ULONG64)MAXULONG_PTR) return STATUS_INVALID_PARAMETER;
    if (physical_address + size < physical_address) return STATUS_INVALID_PARAMETER;

    PHYSICAL_ADDRESS phys_addr;
    phys_addr.QuadPart = physical_address;

    PVOID mapped = MmMapIoSpace(phys_addr, size, MmNonCached);
    if (!mapped) {
        return STATUS_UNSUCCESSFUL;
    }

    __try {
        disable_wp();
        memcpy(mapped, buffer, (size_t)size);
        enable_wp();
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        enable_wp();
        MmUnmapIoSpace(mapped, size);
        return STATUS_UNSUCCESSFUL;
    }

    MmUnmapIoSpace(mapped, size);
    return STATUS_SUCCESS;
}

ULONG64 translate_linear_address(ULONG64 directory_base, ULONG64 virtual_address) {
    if (!directory_base) return 0;
    if (!validate_self_ref_map(virtual_address)) return 0;

    ULONG64 cr3 = directory_base & ~0xFFF;
    ULONG64 va = virtual_address;

    USHORT pml4_idx = (USHORT)((va >> PML4_SHIFT) & PT_INDEX_MASK);
    USHORT pdpt_idx = (USHORT)((va >> PDPT_SHIFT) & PT_INDEX_MASK);
    USHORT pd_idx = (USHORT)((va >> PD_SHIFT) & PT_INDEX_MASK);
    USHORT pt_idx = (USHORT)((va >> PT_SHIFT) & PT_INDEX_MASK);
    USHORT offset = (USHORT)(va & PAGE_OFFSET_MASK);

    ULONG64 pml4e_phys = cr3 + (pml4_idx * 8);
    ULONG64 pml4e = 0;
    if (!NT_SUCCESS(read_physical_memory(pml4e_phys, &pml4e, sizeof(ULONG64)))) return 0;
    if (!(pml4e & 1)) return 0;

    MMPTE_HARDWARE* pml4e_hw = (MMPTE_HARDWARE*)&pml4e;
    if (pml4e_hw->LargePage) {
        return (pml4e_hw->PageFrameNumber << 12) + (va & 0x7FFFFFFFFF);
    }

    ULONG64 pdpt_phys = (pml4e & PHYS_ADDR_MASK) + (pdpt_idx * 8);
    ULONG64 pdpte = 0;
    if (!NT_SUCCESS(read_physical_memory(pdpt_phys, &pdpte, sizeof(ULONG64)))) return 0;
    if (!(pdpte & 1)) return 0;

    MMPTE_HARDWARE* pdpte_hw = (MMPTE_HARDWARE*)&pdpte;
    if (pdpte_hw->LargePage) {
        return (pdpte_hw->PageFrameNumber << 12) + (va & 0x3FFFFFFF);
    }

    ULONG64 pd_phys = (pdpte & PHYS_ADDR_MASK) + (pd_idx * 8);
    ULONG64 pde = 0;
    if (!NT_SUCCESS(read_physical_memory(pd_phys, &pde, sizeof(ULONG64)))) return 0;
    if (!(pde & 1)) return 0;

    MMPTE_HARDWARE* pde_hw = (MMPTE_HARDWARE*)&pde;
    if (pde_hw->LargePage) {
        return (pde_hw->PageFrameNumber << 12) + (va & 0x1FFFFF);
    }

    ULONG64 pt_phys = (pde & PHYS_ADDR_MASK) + (pt_idx * 8);
    ULONG64 pte = 0;
    if (!NT_SUCCESS(read_physical_memory(pt_phys, &pte, sizeof(ULONG64)))) return 0;
    if (!(pte & 1)) return 0;

    MMPTE_HARDWARE* pte_hw = (MMPTE_HARDWARE*)&pte;
    return (pte_hw->PageFrameNumber << 12) + offset;
}

NTSTATUS read_virtual_memory(ULONG64 directory_base, ULONG64 virtual_address, void* buffer, ULONG64 size) {
    if (!buffer || !size) return STATUS_INVALID_PARAMETER;
    if (virtual_address + size < virtual_address) return STATUS_INVALID_PARAMETER;

    ULONG64 bytes_remaining = size;
    ULONG64 current_va = virtual_address;
    unsigned char* dest = (unsigned char*)buffer;

    while (bytes_remaining > 0) {
        ULONG64 page_offset = current_va & PAGE_OFFSET_MASK;
        ULONG64 bytes_in_page = PAGE_SIZE - page_offset;
        ULONG64 bytes_to_read = (bytes_remaining < bytes_in_page) ? bytes_remaining : bytes_in_page;

        ULONG64 phys_addr = translate_linear_address(directory_base, current_va);
        if (!phys_addr) return STATUS_UNSUCCESSFUL;

        NTSTATUS status = read_physical_memory(phys_addr, dest, bytes_to_read);
        if (!NT_SUCCESS(status)) return status;

        dest += bytes_to_read;
        current_va += bytes_to_read;
        bytes_remaining -= bytes_to_read;
    }

    return STATUS_SUCCESS;
}

NTSTATUS write_virtual_memory(ULONG64 directory_base, ULONG64 virtual_address, void* buffer, ULONG64 size) {
    if (!buffer || !size) return STATUS_INVALID_PARAMETER;
    if (virtual_address + size < virtual_address) return STATUS_INVALID_PARAMETER;

    ULONG64 bytes_remaining = size;
    ULONG64 current_va = virtual_address;
    unsigned char* src = (unsigned char*)buffer;

    while (bytes_remaining > 0) {
        ULONG64 page_offset = current_va & PAGE_OFFSET_MASK;
        ULONG64 bytes_in_page = PAGE_SIZE - page_offset;
        ULONG64 bytes_to_write = (bytes_remaining < bytes_in_page) ? bytes_remaining : bytes_in_page;

        ULONG64 phys_addr = translate_linear_address(directory_base, current_va);
        if (!phys_addr) return STATUS_UNSUCCESSFUL;

        NTSTATUS status = write_physical_memory(phys_addr, src, bytes_to_write);
        if (!NT_SUCCESS(status)) return status;

        src += bytes_to_write;
        current_va += bytes_to_write;
        bytes_remaining -= bytes_to_write;
    }

    return STATUS_SUCCESS;
}

ULONG64 resolve_ntoskrnl_base() {
    ULONG64 ntoskrnl_address = 0;
    RTL_OSVERSIONINFOW version_info;
    version_info.dwOSVersionInfoSize = sizeof(RTL_OSVERSIONINFOW);
    RtlGetVersion(&version_info);

    UNICODE_STRING module_name;
    unsigned char enc_module[] = {
        53, 11, 49, 44, 52, 45, 49, 51, 113, 58, 39, 58
    };
    unsigned char key = 0x5B;
    for (int i = 0; i < sizeof(enc_module); i++) {
        enc_module[i] ^= key;
        key = ((key << 3) | (key >> 5)) ^ 0xA5;
    }
    RtlInitUnicodeString(&module_name, (PCWSTR)enc_module);

    PLIST_ENTRY module_list = (PLIST_ENTRY)((ULONG64)PsLoadedModuleList);
    PLIST_ENTRY current = module_list->Flink;

    while (current != module_list) {
        PKLDR_DATA_TABLE_ENTRY entry = CONTAINING_RECORD(current, KLDR_DATA_TABLE_ENTRY, InLoadOrderLinks);
        if (entry->BaseDllName.Buffer && RtlEqualUnicodeString(&entry->BaseDllName, &module_name, TRUE)) {
            ntoskrnl_address = (ULONG64)entry->DllBase;
            break;
        }
        current = current->Flink;
    }

    return ntoskrnl_address;
}

ULONG64 get_kernel_export(const wchar_t* module_name, const char* function_name) {
    UNREFERENCED_PARAMETER(module_name);
    UNREFERENCED_PARAMETER(function_name);
    ULONG64 base = resolve_ntoskrnl_base();
    if (!base) return 0;

    PIMAGE_DOS_HEADER dos_header = (PIMAGE_DOS_HEADER)base;
    PIMAGE_NT_HEADERS nt_headers = (PIMAGE_NT_HEADERS)(base + dos_header->e_lfanew);
    ULONG exports_rva = nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
    if (!exports_rva) return 0;

    PIMAGE_EXPORT_DIRECTORY exports = (PIMAGE_EXPORT_DIRECTORY)(base + exports_rva);
    ULONG* names = (ULONG*)(base + exports->AddressOfNames);
    USHORT* ordinals = (USHORT*)(base + exports->AddressOfNameOrdinals);
    ULONG* functions = (ULONG*)(base + exports->AddressOfFunctions);

    for (ULONG i = 0; i < exports->NumberOfNames; i++) {
        const char* name = (const char*)(base + names[i]);
        if (function_name && name) {
            BOOLEAN match = TRUE;
            for (int j = 0; function_name[j]; j++) {
                if (name[j] != function_name[j]) {
                    match = FALSE;
                    break;
                }
            }
            if (match) {
                return base + functions[ordinals[i]];
            }
        }
    }

    return 0;
}