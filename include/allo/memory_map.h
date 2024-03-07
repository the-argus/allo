#pragma once
#include <cstdint>
#include <errhandlingapi.h>
#if defined(_WIN32)
#include <windows.h>
// LPVOID WINAPI VirtualAlloc(LPVOID lpaddress, SIZE_T size, DWORD
// flAllocationType, DWORD flProtect);
#elif defined(__linux__) || defined(__APPLE__)
#include <errno.h>
#include <sys/mman.h>
#include <unistd.h>
#else
#error "Unsupported OS for memory_map.h"
#endif

struct mm_memory_map_result_t
{
    void *data;
    uint8_t code;
};

/// Get the system's memory page size in bytes.
inline size_t mm_get_page_size()
{
#if defined(_WIN32)
    SYSTEM_INFO sys_info;
    GetSystemInfo(&sys_info);
    return sys_info.dwPageSize;
#elif defined(__linux__)
    return sysconf(_SC_PAGESIZE);
#else
    return getpagesize();
#endif
}

inline mm_memory_map_result_t mm_memory_map(void *address_hint, size_t size)
{
#if defined(_WIN32)
    mm_memory_map_result_t res = (mm_memory_map_result_t){
        .data = VirtualAlloc(address_hint, size, MEM_COMMIT | MEM_RESERVE, 0),
        .code = 0,
    };
    if (res.data == nullptr) {
        res.code = GetLastError();
        if (res.code == 0) {
            res.code = 255;
        }
    }
    return res;
#else
    mm_memory_map_result_t res = (mm_memory_map_result_t){
        .data =
            mmap(address_hint, size, PROT_READ | PROT_WRITE, MAP_ANON, -1, 0),
        .code = 0,
    };
    if (res.data == MAP_FAILED) {
        res.code = errno;
        res.size = 0; // why not
    }
    return res;
#endif
}

inline uint8_t mm_memory_unmap(void *address, size_t size)
{
#if defined(_WIN32)
    if (VirtualFree(address, size, MEM_DECOMMIT | MEM_RELEASE) == 0) {
        return GetLastError();
    }
    return 0;
#else
    if (munmap(address, size) == 0) {
        return 0;
    } else {
        return errno;
    }
#endif
}