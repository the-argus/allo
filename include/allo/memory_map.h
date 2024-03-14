#pragma once
#include <stdint.h>
#if defined(_WIN32)
#include <errhandlingapi.h>
#include <windows.h>
// LPVOID WINAPI VirtualAlloc(LPVOID lpaddress, SIZE_T size, DWORD
// flAllocationType, DWORD flProtect);
#elif defined(__linux__) || defined(__APPLE__)
#include <assert.h>
#include <errno.h>
#include <sys/mman.h>
#include <unistd.h>
#else
#error "Unsupported OS for memory_map.h"
#endif

#ifdef __cplusplus
extern "C"
{
#endif

    struct mm_memory_map_result_t
    {
        void *data;
        size_t bytes;
        uint8_t code;
    };

    struct mm_memory_map_resize_options_t
    {
        void *address;
        size_t old_size;
        size_t new_size;
    };

    struct mm_optional_uint64_t
    {
        uint64_t value : 63;
        uint64_t has_value : 1;
    };

    /// Get the system's memory page size in bytes.
    /// Can fail on linux, in which case the returned optional uint has its
    /// has_value bit set to 0.
    inline mm_optional_uint64_t mm_get_page_size()
    {
#if defined(_WIN32)
        SYSTEM_INFO sys_info;
        GetSystemInfo(&sys_info);
        return sys_info.dwPageSize;
#elif defined(__linux__)
    long result = sysconf(_SC_PAGESIZE);
    if (result < 0) {
        return (mm_optional_uint64_t){.value = 0, .has_value = 0};
    }
    return (mm_optional_uint64_t){.value = (uint64_t)result, .has_value = 1};
#else
    return getpagesize();
#endif
    }

    /// Reserve a number of pages in virtual memory space. You cannot write to
    /// memory allocated by this function. Call mm_commit_pages on each page
    /// you want to write to first.
    /// Address hint: a hint for where the OS should try to place the
    /// reservation.
    /// Num pages: the number of pages to reserve.
    inline mm_memory_map_result_t mm_reserve_pages(void *address_hint,
                                                   size_t num_pages)
    {
        const mm_optional_uint64_t result = mm_get_page_size();
        if (!result.has_value) {
            // code 254 means unable to get page size... unlikely
            return (mm_memory_map_result_t){.code = 254};
        }
        size_t size = num_pages * result.value;
#if defined(_WIN32)
        mm_memory_map_result_t res = (mm_memory_map_result_t){
            .data = VirtualAlloc(address_hint, size, MEM_RESERVE, 0),
            .bytes = size,
            .code = 0,
        };
        if (res.data == NULL) {
            res.code = GetLastError();
            if (res.code == 0) {
                res.code = 255;
            }
        }
        return res;
#else
    mm_memory_map_result_t res = (mm_memory_map_result_t){
        .data =
            mmap(address_hint, size, PROT_NONE, MAP_ANON | MAP_PRIVATE, -1, 0),
        .bytes = size,
        .code = 0,
    };
    if (res.data == MAP_FAILED) {
        res.code = errno;
        res.data = NULL;
    }
    return res;
#endif
    }

    /// Takes a pointer to a set of memory pages which you want to make
    /// readable and writable by your process, specifically ones allocated by
    /// mm_reserve_pages().
    /// Returns -1 if it fails, or 1 otherwise.
    inline int8_t mm_commit_pages(void *address, size_t num_pages)
    {
        if (!address) {
            return -1;
        }
        const mm_optional_uint64_t result = mm_get_page_size();
        if (!result.has_value) {
            return -1;
        }

        size_t size = num_pages * result.value;
#if defined(_WIN32)
        mm_memory_map_result_t res = (mm_memory_map_result_t){
            .data = VirtualAlloc(address_hint, size, MEM_RESERVE, 0),
            .bytes = size,
            .code = 0,
        };
        if (res.data == NULL) {
            res.code = GetLastError();
            if (res.code == 0) {
                res.code = 255;
            }
        }
        return res;
#else
    if (mprotect(address, size, (PROT_READ | PROT_WRITE) == -1)) {
        // failure case, you probably passed in an address that is not page
        // aligned.
        return -1;
    }
    return 1;
#endif
    }

    /// Unmap pages starting at address and continuing for "size" bytes.
    inline int mm_memory_unmap(void *address, size_t size)
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

    /*
    inline int
    mm_memory_map_resize(const mm_memory_map_resize_options_t *options)
    {
#if defined(_WIN32)
#error "currently unsupported"
        // NOTE: use
        // https://stackoverflow.com/questions/17197615/no-mremap-for-windows
        // also see:
        //
https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-mapuserphysicalpages
#else
    void *res =
        mremap(options->address, options->old_size, options->new_size, 0);
    if (res == MAP_FAILED) {
        return errno;
    }
    assert(res == options->address);
    return 0;
#endif
    }
    */

#ifdef __cplusplus
}
#endif
