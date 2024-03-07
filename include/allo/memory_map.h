#pragma once
#if defined(_WIN32)
#include <windows.h>
// LPVOID WINAPI VirtualAlloc(LPVOID lpaddress, SIZE_T size, DWORD
// flAllocationType, DWORD flProtect);
#elif defined(__linux__) || defined(__APPLE__)
#include <sys/mman.h>
#endif

#if defined(_WIN32)
#elif defined(__linux__) || defined(__APPLE__)
#endif
