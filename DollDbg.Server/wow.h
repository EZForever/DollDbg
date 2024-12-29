#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

extern "C"
{
#if defined(_M_IX86)
    constexpr USHORT IMAGE_PROCESS_MACHINE = IMAGE_FILE_MACHINE_I386;
#elif defined(_M_AMD64)
    constexpr USHORT IMAGE_PROCESS_MACHINE = IMAGE_FILE_MACHINE_AMD64;
#elif defined(_M_ARM)
    constexpr USHORT IMAGE_PROCESS_MACHINE = IMAGE_FILE_MACHINE_ARM;
#elif defined(_M_ARM64)
    constexpr USHORT IMAGE_PROCESS_MACHINE = IMAGE_FILE_MACHINE_ARM64;
#endif

    USHORT xGetProcessMachine(HANDLE hProcess);
}