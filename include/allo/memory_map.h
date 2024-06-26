#pragma once
#include <assert.h>
#include <stdint.h>

#if defined(_WIN32)
#include <errhandlingapi.h>
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

#ifdef __cplusplus
extern "C"
{
#endif

    struct mm_memory_map_result_t
    {
        void* data;
        size_t bytes;
        /// Error-code. This will be 0 on success: check that before reading
        /// bytes and data. There is no compatibility of error codes between
        /// OSes. The only guaranteed is that it will be 0 on success on all
        /// platforms.
        int64_t code;
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
        return (mm_optional_uint64_t){.value = sys_info.dwPageSize,
                                      .has_value = 1};
#elif defined(__linux__)
    long result = sysconf(_SC_PAGESIZE);
    if (result < 0) {
        return (mm_optional_uint64_t){.value = 0, .has_value = 0};
    }
    return (mm_optional_uint64_t){.value = (uint64_t)result, .has_value = 1};
#else
    return (mm_optional_uint64_t){.value = getpagesize(), .has_value = 1};
#endif
    }

    /// Reserve a number of pages in virtual memory space. You cannot write to
    /// memory allocated by this function. Call mm_commit_pages on each page
    /// you want to write to first.
    /// Address hint: a hint for where the OS should try to place the
    /// reservation. May be ignored if there is not space there. nullptr
    /// can be provided to let the OS choose where to place the reservation.
    /// Note that it's possible to map memory directly after the stack and
    /// cause horrible memory bugs when the stack overruns your allocation.
    /// Num pages: the number of pages to reserve.
    inline mm_memory_map_result_t mm_reserve_pages(void* address_hint,
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
            assert(res.code != 0);
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
    /// Returns 0 on success, otherwise an errcode.
    inline int64_t mm_commit_pages(void* address, size_t num_pages)
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
        int64_t err = 0;
        if (!VirtualAlloc(address, size, MEM_RESERVE | MEM_COMMIT, 0)) {
            err = GetLastError();
            assert(err != 0);
        }
        return err;
#else
    if (mprotect(address, size, PROT_READ | PROT_WRITE) != 0) {
        // failure case, you probably passed in an address that is not page
        // aligned.
        int32_t res = errno;
        assert(res != 0);
        return res;
    }
    return 0;
#endif
    }

    /// Unmap pages starting at address and continuing for "size" bytes.
    inline int64_t mm_memory_unmap(void* address, size_t size)
    {
#if defined(_WIN32)
        int64_t err = 0;
        if (!VirtualFree(address, size, MEM_DECOMMIT | MEM_RELEASE)) {
            err = GetLastError();
        }
        return err;
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
