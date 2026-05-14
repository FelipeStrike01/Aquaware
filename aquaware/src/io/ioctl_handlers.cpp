#include "../../include/io/ioctl_handlers.h"
#include "../../include/common/driver_types.h"
#include "../../include/core/state.h"
#include "../../include/core/validation.h"
#include "../../include/process/process_utils.h"
#include "../helpers.h"
#include <ntddk.h>

extern volatile COMMAND_BUFFER* g_shared_buffer;
extern volatile LONG g_initialized;

NTSTATUS dispatch_create(PDEVICE_OBJECT device, PIRP irp) {
    UNREFERENCED_PARAMETER(device);
    irp->IoStatus.Status = STATUS_SUCCESS;
    irp->IoStatus.Information = 0;
    IoCompleteRequest(irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

NTSTATUS dispatch_close(PDEVICE_OBJECT device, PIRP irp) {
    UNREFERENCED_PARAMETER(device);
    irp->IoStatus.Status = STATUS_SUCCESS;
    irp->IoStatus.Information = 0;
    IoCompleteRequest(irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

NTSTATUS dispatch_control(PDEVICE_OBJECT device, PIRP irp) {
    UNREFERENCED_PARAMETER(device);

    if (KeGetCurrentIrql() > APC_LEVEL) {
        irp->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;
        irp->IoStatus.Information = 0;
        IoCompleteRequest(irp, IO_NO_INCREMENT);
        return STATUS_INVALID_DEVICE_REQUEST;
    }

    if (!InterlockedCompareExchange((LONG*)&g_initialized, 1, 1)) {
        irp->IoStatus.Status = STATUS_DEVICE_NOT_READY;
        irp->IoStatus.Information = 0;
        IoCompleteRequest(irp, IO_NO_INCREMENT);
        return STATUS_DEVICE_NOT_READY;
    }

    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(irp);
    ULONG control_code = stack->Parameters.DeviceIoControl.IoControlCode;
    KPROCESSOR_MODE mode = ExGetPreviousMode();

    switch (control_code) {
        case 0x800: {
            if (stack->Parameters.DeviceIoControl.InputBufferLength >= sizeof(HANDLE)) {
                HANDLE pid = *(HANDLE*)irp->AssociatedIrp.SystemBuffer;
                irp->IoStatus.Status = set_active_process(pid);
            } else {
                irp->IoStatus.Status = STATUS_BUFFER_TOO_SMALL;
            }
            break;
        }
        case 0x801: {
            if (g_shared_buffer) {
                g_shared_buffer->command_id = CMD_READ_VIRTUAL;
                g_shared_buffer->status = STATUS_PENDING;
                if (stack->Parameters.DeviceIoControl.Type3InputBuffer &&
                    validate_user_range(stack->Parameters.DeviceIoControl.Type3InputBuffer,
                                        stack->Parameters.DeviceIoControl.InputBufferLength, mode)) {
                    ULONG copy_len = (stack->Parameters.DeviceIoControl.InputBufferLength < sizeof(ULONG64))
                                         ? stack->Parameters.DeviceIoControl.InputBufferLength
                                         : sizeof(ULONG64);
                    memcpy((PVOID)&g_shared_buffer->address, stack->Parameters.DeviceIoControl.Type3InputBuffer,
                           copy_len);
                } else {
                    irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
                    break;
                }
                LARGE_INTEGER timeout;
                timeout.QuadPart = -10000000;
                while (g_shared_buffer->status == STATUS_PENDING) {
                    KeDelayExecutionThread(KernelMode, FALSE, &timeout);
                }
                if (stack->Parameters.DeviceIoControl.OutputBufferLength >= sizeof(ULONG64)) {
                    *(ULONG64*)irp->AssociatedIrp.SystemBuffer = g_shared_buffer->result;
                    irp->IoStatus.Information = sizeof(ULONG64);
                }
            }
            irp->IoStatus.Status = STATUS_SUCCESS;
            break;
        }
        case 0x802: {
            if (g_shared_buffer) {
                g_shared_buffer->command_id = CMD_GET_MODULE_BASE;
                g_shared_buffer->status = STATUS_PENDING;
                if (stack->Parameters.DeviceIoControl.Type3InputBuffer &&
                    validate_user_range(stack->Parameters.DeviceIoControl.Type3InputBuffer,
                                        stack->Parameters.DeviceIoControl.InputBufferLength, mode)) {
                    ULONG copy_len = (stack->Parameters.DeviceIoControl.InputBufferLength < sizeof(ULONG64))
                                         ? stack->Parameters.DeviceIoControl.InputBufferLength
                                         : sizeof(ULONG64);
                    memcpy((PVOID)&g_shared_buffer->address, stack->Parameters.DeviceIoControl.Type3InputBuffer,
                           copy_len);
                } else {
                    irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
                    break;
                }
                LARGE_INTEGER timeout;
                timeout.QuadPart = -10000000;
                while (g_shared_buffer->status == STATUS_PENDING) {
                    KeDelayExecutionThread(KernelMode, FALSE, &timeout);
                }
                if (stack->Parameters.DeviceIoControl.OutputBufferLength >= sizeof(ULONG64)) {
                    *(ULONG64*)irp->AssociatedIrp.SystemBuffer = g_shared_buffer->result;
                    irp->IoStatus.Information = sizeof(ULONG64);
                }
            }
            irp->IoStatus.Status = STATUS_SUCCESS;
            break;
        }
        default:
            irp->IoStatus.Status = STATUS_NOT_SUPPORTED;
            break;
    }

    irp->IoStatus.Information = (irp->IoStatus.Information == 0) ? 0 : irp->IoStatus.Information;
    IoCompleteRequest(irp, IO_NO_INCREMENT);
    return irp->IoStatus.Status;
}