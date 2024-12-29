#include "xdetours.h"

extern "C"
{
    // DetourCodeFromPointer() doesn't work for a given "jmp imm32"
    PVOID WINAPI xDetourCodeFromPointer(PVOID pPointer, PVOID* ppGlobals)
    {
        PBYTE pbCode = (PBYTE)DetourCodeFromPointer(pPointer, ppGlobals);
#if defined(DETOURS_X86) || defined(DETOURS_X64)
        if (pbCode[0] == 0xe9)
            pbCode = pbCode + 5 + *(UNALIGNED INT32*)&pbCode[1];
#endif
        return pbCode;
    }
}