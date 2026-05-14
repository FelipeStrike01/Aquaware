#pragma once
#include <ntddk.h>

#define POOL_TAG 0x48736157
#define SHARED_MEMORY_SIZE 0x10000
#define PAGE_SIZE 0x1000
#define PAGE_MASK (~(PAGE_SIZE - 1))
#define PAGE_OFFSET_MASK (PAGE_SIZE - 1)
#define PT_INDEX_MASK 0x1FF
#define PML4_SHIFT 39
#define PDPT_SHIFT 30
#define PD_SHIFT 21
#define PT_SHIFT 12
#define PHYS_ADDR_MASK 0x0000FFFFFFFFF000
#define SELF_REF_IDX 0x1ED
#define VIRTUAL_ADDRESS_BITS 48
#define SIGN_EXTEND_MASK 0xFFFF000000000000

struct COMMAND_BUFFER {
    volatile ULONG command_id;
    volatile ULONG status;
    volatile ULONG64 address;
    volatile ULONG64 buffer_address;
    volatile ULONG64 size;
    volatile ULONG64 result;
    volatile ULONG64 cr3;
};

typedef enum _COMMAND_IDS {
    CMD_NONE = 0,
    CMD_READ_PHYSICAL = 1,
    CMD_READ_VIRTUAL = 2,
    CMD_WRITE_PHYSICAL = 3,
    CMD_WRITE_VIRTUAL = 4,
    CMD_GET_MODULE_BASE = 5,
    CMD_GET_PROCESS_CR3 = 6,
    CMD_GET_PID_BY_NAME = 7,
    CMD_READ_BATCH = 8,
    CMD_WRITE_BATCH = 9
} COMMAND_IDS;

#define CMD_STATUS_IDLE 0
#define CMD_STATUS_PENDING 1
#define CMD_STATUS_COMPLETE 2
#define CMD_STATUS_ERROR 3
#define STATUS_IDLE 0

typedef struct _MMPTE_HARDWARE {
    ULONG64 Valid : 1;
    ULONG64 Write : 1;
    ULONG64 Owner : 1;
    ULONG64 WriteThrough : 1;
    ULONG64 CacheDisable : 1;
    ULONG64 Accessed : 1;
    ULONG64 Dirty : 1;
    ULONG64 LargePage : 1;
    ULONG64 Global : 1;
    ULONG64 CopyOnWrite : 1;
    ULONG64 Prototype : 1;
    ULONG64 reserved0 : 1;
    ULONG64 PageFrameNumber : 36;
    ULONG64 reserved1 : 4;
    ULONG64 SoftwareWsIndex : 11;
    ULONG64 NoExecute : 1;
} MMPTE_HARDWARE;

struct BATCH_ENTRY {
    ULONG64 address;
    ULONG64 buffer_address;
    ULONG64 size;
    ULONG64 bytes_read;
    ULONG status;
};

struct BATCH_REQUEST {
    BATCH_ENTRY* entries;
    ULONG count;
    ULONG64 cr3;
};