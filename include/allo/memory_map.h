#pragma once
#include <cstdint>
#if defined(_WIN32)
#include <windows.h>
// LPVOID WINAPI VirtualAlloc(LPVOID lpaddress, SIZE_T size, DWORD
// flAllocationType, DWORD flProtect);
#elif defined(__linux__) || defined(__APPLE__)
#include <errno.h>
#include <sys/mman.h>
#endif

struct memory_map_result_t
{
    void *data;
    size_t size;
    uint8_t code;
    [[nodiscard]] inline constexpr bool okay() const noexcept
    {
        return code == 0;
    }
};

/// ON WINDOWS:
///  - size is rounded up to nearest page boundary if address is NULL
/// ON LINUX:
///  - size stays the same as requested
inline memory_map_result_t memory_map(void *address_hint, size_t size)
{
#if defined(_WIN32)
    return (void *)VirtualAlloc((LPVOID)address_hint, size,
                                MEM_COMMIT | MEM_RESERVE, 0);
#elif defined(__linux__) || defined(__APPLE__)
    auto res = memory_map_result_t{
        .data =
            mmap(address_hint, size, PROT_READ | PROT_WRITE, MAP_ANON, -1, 0),
        .size = size,
        .code = 0,
    };
    if (res.data == MAP_FAILED) {
        res.code = errno;
        res.size = 0; // why not
    }
    return res;
#endif
}

inline uint8_t memory_unmap(void *address, size_t size)
{
#if defined(_WIN32)
#elif defined(__linux__) || defined(__APPLE__)
    if (munmap(address, size) == 0) {
        return 0;
    } else {
        return errno;
    }
#endif
}
