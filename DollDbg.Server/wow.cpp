#include "wow.h"

extern "C"
{
    // FIXME: This will break on WoA < Windows 10 v1511
    USHORT xGetProcessMachine(HANDLE hProcess)
    {
        static auto pIsWow64Process = (decltype(IsWow64Process)*)GetProcAddress(GetModuleHandle(TEXT("kernel32")), "IsWow64Process");
        static auto pIsWow64Process2 = (decltype(IsWow64Process2)*)GetProcAddress(GetModuleHandle(TEXT("kernel32")), "IsWow64Process2");
        static USHORT nativeMachine = IMAGE_FILE_MACHINE_UNKNOWN;

        USHORT processMachine;
        BOOL isWow64;

        // Ancient 32-bit systems does not support IsWow64Process(), so do not even bother trying
        if (!pIsWow64Process)
            return IMAGE_FILE_MACHINE_I386;

        // Initialize nativeMachine
        // NOTE: Do not just query for processor architecture (constant translation / WoW'd system)
        if (nativeMachine == IMAGE_FILE_MACHINE_UNKNOWN)
        {
            if (pIsWow64Process2)
            {
                if (!pIsWow64Process2(GetCurrentProcess(), &processMachine, &nativeMachine))
                    return IMAGE_FILE_MACHINE_UNKNOWN;
            }
            else
            {
                // The logic:
                // Without IsWow64Process2() we cannot differentiate between WoW64 and WoA (IsWow64GuestMachineSupported() came out even later)
                // We have to assume it's WoW64 since currently DollDbg HookLL is not implemented for ARM processors
                // If the process is WoW64, it has to be x86 emulated on x64 (who the hecc is using IA64 anyway); otherwise it's native
                // We get native machine type by querying ourselves: has to be x64 if WoW64, the same as this image otherwise
                
                if(!pIsWow64Process(GetCurrentProcess(), &isWow64))
                    return IMAGE_FILE_MACHINE_UNKNOWN;
                nativeMachine = isWow64 ? IMAGE_FILE_MACHINE_AMD64 : IMAGE_PROCESS_MACHINE;
            }
        }

        if (pIsWow64Process2)
        {
            if (!pIsWow64Process2(hProcess, &processMachine, NULL))
                return IMAGE_FILE_MACHINE_UNKNOWN;

            return processMachine == IMAGE_FILE_MACHINE_UNKNOWN ? nativeMachine : processMachine;
        }
        else
        {
            if(!pIsWow64Process(hProcess, &isWow64))
                return IMAGE_FILE_MACHINE_UNKNOWN;

            return isWow64 ? IMAGE_FILE_MACHINE_I386 : nativeMachine;
        }
    }
}