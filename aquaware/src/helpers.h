#pragma once
#include <ntddk.h>
#include <intrin.h>

namespace {
    inline ULONG64 read_cr3() {
        return __readcr3();
    }

    inline void write_cr3(ULONG64 value) {
        __writecr3(value);
    }

    inline ULONG64 read_cr8() {
        return __readcr8();
    }

    inline void disable_wp() {
        __writecr0(__readcr0() & ~0x10000);
    }

    inline void enable_wp() {
        __writecr0(__readcr0() | 0x10000);
    }

    inline void invlpg(void* address) {
        __invlpg(address);
    }

    inline void flush_tlb() {
        ULONG64 cr3 = read_cr3();
        write_cr3(cr3);
    }

    inline ULONG64 get_physical_address(void* virtual_address) {
        return MmGetPhysicalAddress(virtual_address).QuadPart;
    }

    inline void* map_physical_address(ULONG64 physical_address, ULONG64 size) {
        PHYSICAL_ADDRESS phys_addr;
        phys_addr.QuadPart = physical_address;
        return MmMapIoSpace(phys_addr, size, MmNonCached);
    }

    inline void unmap_physical_address(void* virtual_address, ULONG64 size) {
        MmUnmapIoSpace(virtual_address, size);
    }
}