#pragma once
#include <ntddk.h>

NTSTATUS dispatch_create(PDEVICE_OBJECT device, PIRP irp);
NTSTATUS dispatch_close(PDEVICE_OBJECT device, PIRP irp);
NTSTATUS dispatch_control(PDEVICE_OBJECT device, PIRP irp);