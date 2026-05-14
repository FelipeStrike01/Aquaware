#include "../include/common/driver_types.h"
#include "../include/core/logging.h"
#include "../include/core/state.h"
#include "../include/core/validation.h"
#include "../include/io/ioctl_handlers.h"
#include "../include/process/process_utils.h"
#include <ntddk.h>
#include <wdm.h>
#include <intrin.h>

#pragma comment(lib, "ntoskrnl.lib")

extern "C" POBJECT_TYPE* MmSectionObjectType;

// Global variables (exported)
volatile COMMAND_BUFFER* g_shared_buffer = nullptr;
volatile LONG g_initialized = 0;

namespace {
    PMDL g_shared_mdl = nullptr;
    PVOID g_shared_user = nullptr;
    PVOID g_shared_kernel = nullptr;
    HANDLE g_section_handle = nullptr;
    PDEVICE_OBJECT g_device = nullptr;
    UNICODE_STRING* g_device_name = nullptr;
    UNICODE_STRING* g_link_name = nullptr;
    FAST_MUTEX g_shared_lock;
    volatile LONG g_command_in_progress = 0;

    unsigned char g_decrypt_buffer[256];

    void init_decrypt_buffer() {
        for (int i = 0; i < 256; i++) {
            g_decrypt_buffer[i] = (unsigned char)(((i * 0x5D) ^ 0xA5) + (i >> 2));
        }
    }

    void decrypt_data(unsigned char* data, ULONG size, ULONG key) {
        if (!data || !size) return;
        init_decrypt_buffer();
        unsigned char state = (unsigned char)(key & 0xFF);
        for (ULONG i = 0; i < size; i++) {
            state ^= g_decrypt_buffer[i & 0xFF];
            data[i] ^= state;
            state = ((state << 3) | (state >> 5)) ^ g_decrypt_buffer[(i + key) & 0xFF];
        }
    }

    UNICODE_STRING* build_unicode_string(const unsigned char* encrypted, ULONG length, ULONG key) {
        UNICODE_STRING* str = (UNICODE_STRING*)ExAllocatePool2(POOL_FLAG_NON_PAGED, sizeof(UNICODE_STRING), POOL_TAG);
        if (!str) return nullptr;
        wchar_t* buffer = (wchar_t*)ExAllocatePool2(POOL_FLAG_NON_PAGED, length + 2, POOL_TAG);
        if (!buffer) {
            ExFreePoolWithTag(str, POOL_TAG);
            return nullptr;
        }
        memcpy(buffer, encrypted, length);
        decrypt_data((unsigned char*)buffer, length, key);
        buffer[length / 2] = L'\0';
        RtlInitUnicodeString(str, buffer);
        return str;
    }

    void free_unicode_string(UNICODE_STRING* str) {
        if (str) {
            if (str->Buffer) ExFreePoolWithTag(str->Buffer, POOL_TAG);
            ExFreePoolWithTag(str, POOL_TAG);
        }
    }

    unsigned char g_device_name_enc[] = {
        131, 87, 234, 240, 108, 252, 176, 200, 98, 145, 192, 186, 120, 137, 245, 163, 97, 186, 84, 35, 160
    };

    unsigned char g_link_name_enc[] = {
        181, 191, 224, 59, 85, 33, 220, 24, 65, 72, 218, 77, 22, 227, 40, 103, 46, 212, 62, 79, 70, 202, 72, 173, 180
    };

    unsigned char g_sddl_enc[] = {
        219, 251, 185, 157, 181, 147, 159, 96, 191, 73, 203, 186, 150, 100, 25, 151, 247, 19, 93, 182, 55, 207, 141, 12, 68, 140, 156, 187, 52
    };

    const GUID g_device_guid = {
        0x9C3B7E1A, 0x5D48, 0x4F2C,
        {0xA8, 0x9E, 0x1F, 0x4C, 0x7D, 0x3B, 0x2E, 0x6A}
    };

    NTSTATUS allocate_shared_memory() {
        UNICODE_STRING section_name;
        unsigned char section_name_enc[] = {
            183, 227, 106, 240, 55, 66, 139, 171, 141, 193, 168, 178, 110, 175, 235, 177, 119, 178, 176, 145, 205, 202, 152, 180, 131, 228, 162, 110, 151, 222, 74, 193, 213, 150, 141, 153, 229, 167, 155, 132, 240, 46, 179
        };
        decrypt_data(section_name_enc, sizeof(section_name_enc), 0x7F3A9C4E);
        RtlInitUnicodeString(&section_name, (PCWSTR)section_name_enc);

        OBJECT_ATTRIBUTES obj_attr;
        InitializeObjectAttributes(&obj_attr, &section_name, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, nullptr, nullptr);

        LARGE_INTEGER section_size;
        section_size.QuadPart = SHARED_MEMORY_SIZE;

        NTSTATUS status = ZwCreateSection(
            &g_section_handle,
            SECTION_MAP_READ | SECTION_MAP_WRITE,
            &obj_attr,
            &section_size,
            PAGE_READWRITE,
            SEC_COMMIT,
            nullptr
        );

        if (!NT_SUCCESS(status)) return status;

        status = ObReferenceObjectByHandle(
            g_section_handle,
            SECTION_MAP_READ | SECTION_MAP_WRITE,
            *MmSectionObjectType,
            KernelMode,
            (PVOID*)&g_shared_kernel,
            nullptr
        );

        if (!NT_SUCCESS(status)) {
            ZwClose(g_section_handle);
            return status;
        }

        g_shared_buffer = (COMMAND_BUFFER*)g_shared_kernel;
        RtlZeroMemory((PVOID)g_shared_buffer, SHARED_MEMORY_SIZE);
        g_shared_buffer->status = STATUS_IDLE;

        return STATUS_SUCCESS;
    }

    NTSTATUS map_shared_memory() {
        SIZE_T view_size = SHARED_MEMORY_SIZE;
        NTSTATUS status = ZwMapViewOfSection(
            g_section_handle,
            ZwCurrentProcess(),
            &g_shared_user,
            0,
            SHARED_MEMORY_SIZE,
            nullptr,
            &view_size,
            ViewShare,
            0,
            PAGE_READWRITE
        );

        return status;
    }

    void process_command() {
        if (!g_shared_buffer) return;
        if (KeGetCurrentIrql() > APC_LEVEL) return;
        if (InterlockedCompareExchange(&g_command_in_progress, 1, 0) != 0) return;

        ExAcquireFastMutex(&g_shared_lock);

        COMMAND_BUFFER cmd;
        RtlZeroMemory(&cmd, sizeof(COMMAND_BUFFER));

        __try {
            memcpy(&cmd, (PVOID)g_shared_buffer, sizeof(COMMAND_BUFFER));
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            ExReleaseFastMutex(&g_shared_lock);
            InterlockedExchange(&g_command_in_progress, 0);
            return;
        }

        if (cmd.command_id == CMD_NONE || cmd.status != CMD_STATUS_PENDING) {
            ExReleaseFastMutex(&g_shared_lock);
            InterlockedExchange(&g_command_in_progress, 0);
            return;
        }

        cmd.result = 0;
        cmd.status = CMD_STATUS_COMPLETE;

        BOOLEAN valid = TRUE;
        if (cmd.command_id == CMD_READ_PHYSICAL || cmd.command_id == CMD_READ_VIRTUAL) {
            if (!cmd.size || cmd.size > SHARED_MEMORY_SIZE - sizeof(COMMAND_BUFFER)) {
                valid = FALSE;
            }
        }
        if (cmd.command_id == CMD_READ_PHYSICAL) {
            if (!validate_physical_address(cmd.address)) valid = FALSE;
            if (cmd.address + cmd.size < cmd.address) valid = FALSE;
        }
        if (cmd.command_id == CMD_READ_VIRTUAL) {
            if (!cmd.cr3 || !validate_self_ref_map(cmd.address)) valid = FALSE;
        }

        if (!valid) {
            cmd.status = CMD_STATUS_ERROR;
        } else {
            __try {
                switch (cmd.command_id) {
                    case CMD_READ_PHYSICAL: {
                        void* dest = (PVOID)((ULONG64)g_shared_buffer + sizeof(COMMAND_BUFFER));
                        NTSTATUS s = read_physical_memory(cmd.address, dest, cmd.size);
                        cmd.result = (ULONG64)s;
                        break;
                    }
                    case CMD_READ_VIRTUAL: {
                        ULONG64 phys_addr = translate_linear_address(cmd.cr3, cmd.address);
                        if (!phys_addr) {
                            cmd.status = CMD_STATUS_ERROR;
                            break;
                        }
                        void* dest = (PVOID)((ULONG64)g_shared_buffer + sizeof(COMMAND_BUFFER));
                        NTSTATUS s = read_physical_memory(phys_addr, dest, cmd.size);
                        cmd.result = (ULONG64)s;
                        break;
                    }
                    case CMD_GET_MODULE_BASE: {
                        cmd.result = get_kernel_export((const wchar_t*)cmd.address, nullptr);
                        break;
                    }
                    default:
                        cmd.status = CMD_STATUS_ERROR;
                        break;
                }
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                cmd.status = CMD_STATUS_ERROR;
            }
        }

        __try {
            memcpy((PVOID)g_shared_buffer, &cmd, sizeof(COMMAND_BUFFER));
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
        }

        ExReleaseFastMutex(&g_shared_lock);
        InterlockedExchange(&g_command_in_progress, 0);
    }

    VOID shared_memory_callback(PVOID context, PVOID arg1, PVOID arg2) {
        UNREFERENCED_PARAMETER(context);
        UNREFERENCED_PARAMETER(arg1);
        UNREFERENCED_PARAMETER(arg2);
        process_command();
    }
}

extern "C" DRIVER_INITIALIZE DriverEntry;
extern "C" DRIVER_UNLOAD DriverUnload;

NTSTATUS dispatch_routine(PDEVICE_OBJECT device, PIRP irp) {
    UNREFERENCED_PARAMETER(device);
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(irp);

    switch (stack->MajorFunction) {
        case IRP_MJ_CREATE:
            irp->IoStatus.Status = STATUS_SUCCESS;
            break;
        case IRP_MJ_CLOSE:
            irp->IoStatus.Status = STATUS_SUCCESS;
            break;
        case IRP_MJ_DEVICE_CONTROL:
            if (!InterlockedCompareExchange((LONG*)&g_initialized, 1, 1) || !g_shared_buffer) {
                irp->IoStatus.Status = STATUS_DEVICE_NOT_READY;
                break;
            }
            if (KeGetCurrentIrql() > APC_LEVEL) {
                irp->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;
                break;
            }
            process_command();
            irp->IoStatus.Status = STATUS_SUCCESS;
            break;
        default:
            irp->IoStatus.Status = STATUS_NOT_SUPPORTED;
            break;
    }

    irp->IoStatus.Information = 0;
    IoCompleteRequest(irp, IO_NO_INCREMENT);
    return irp->IoStatus.Status;
}

VOID DriverUnload(PDRIVER_OBJECT driver) {
    UNREFERENCED_PARAMETER(driver);
    InterlockedExchange(&g_initialized, 0);
    KeMemoryBarrier();

    if (g_shared_buffer) {
        g_shared_buffer->status = CMD_STATUS_ERROR;
    }

    if (g_shared_user) {
        ZwUnmapViewOfSection(ZwCurrentProcess(), g_shared_user);
        g_shared_user = nullptr;
    }

    if (g_section_handle) {
        ZwClose(g_section_handle);
        g_section_handle = nullptr;
    }

    cleanup_state();

    if (g_link_name) {
        IoDeleteSymbolicLink(g_link_name);
        free_unicode_string(g_link_name);
    }

    if (g_device) {
        IoDeleteDevice(g_device);
    }

    if (g_device_name) {
        free_unicode_string(g_device_name);
    }
}

NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING registry) {
    UNREFERENCED_PARAMETER(registry);
    NTSTATUS status;

    ExInitializeFastMutex(&g_shared_lock);
    init_state();

    g_device_name = build_unicode_string(g_device_name_enc, sizeof(g_device_name_enc), 0x5B9E2C7A);
    g_link_name = build_unicode_string(g_link_name_enc, sizeof(g_link_name_enc), 0x3F8A1D4C);

    if (!g_device_name || !g_link_name) {
        DriverUnload(driver);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    UNICODE_STRING sddl;
    wchar_t* sddl_buffer = (wchar_t*)ExAllocatePool2(POOL_FLAG_NON_PAGED, sizeof(g_sddl_enc) + 2, POOL_TAG);
    if (!sddl_buffer) {
        DriverUnload(driver);
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    memcpy(sddl_buffer, g_sddl_enc, sizeof(g_sddl_enc));
    decrypt_data((unsigned char*)sddl_buffer, sizeof(g_sddl_enc), 0x6C1F9E3A);
    sddl_buffer[sizeof(g_sddl_enc) / 2] = L'\0';
    RtlInitUnicodeString(&sddl, sddl_buffer);

    status = IoCreateDevice(
        driver,
        0,
        g_device_name,
        FILE_DEVICE_UNKNOWN,
        FILE_DEVICE_SECURE_OPEN,
        FALSE,
        &g_device
    );

    ExFreePoolWithTag(sddl_buffer, POOL_TAG);

    if (!NT_SUCCESS(status)) {
        DriverUnload(driver);
        return status;
    }

    status = IoCreateSymbolicLink(g_link_name, g_device_name);
    if (!NT_SUCCESS(status)) {
        DriverUnload(driver);
        return status;
    }

    status = allocate_shared_memory();
    if (!NT_SUCCESS(status)) {
        DriverUnload(driver);
        return status;
    }

    driver->MajorFunction[IRP_MJ_CREATE] = dispatch_routine;
    driver->MajorFunction[IRP_MJ_CLOSE] = dispatch_routine;
    driver->MajorFunction[IRP_MJ_DEVICE_CONTROL] = dispatch_routine;
    driver->DriverUnload = DriverUnload;

    InterlockedExchange(&g_initialized, 1);
    return STATUS_SUCCESS;
}