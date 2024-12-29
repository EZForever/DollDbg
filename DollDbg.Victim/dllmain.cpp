#include "victim.h"

#include <dolldbg/util/exc.h>

#include <detours.h>

using namespace DollDbg;

extern "C"
{
    BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
    {
        switch (ul_reason_for_call)
        {
        case DLL_PROCESS_ATTACH:
        {
            BOOL isStatic = DetourRestoreAfterWith();
            try
            {
                Victim::Victim::init(hModule, isStatic);
            }
            catch (...)
            {
                OutputDebugString(Util::string_from_exc().c_str());
                return FALSE;
            }
            return TRUE;
        }
        case DLL_PROCESS_DETACH:
        {
#if 0 // XXX: Just... do not do this. at the time of DLL_PROCESS_DETACH, all RPC threads are already dead
            try
            {
                Victim::Victim::fini();
            }
            catch (...)
            {
                OutputDebugString(Util::string_from_exc().c_str());
                return FALSE; // Ignored by the system
            }
#endif
            return TRUE;
        }
        //case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
        {
            try
            {
                Victim::Victim::get().hookHLThreadFini();
            }
            catch (...)
            {
                OutputDebugString(Util::string_from_exc().c_str());
                return FALSE;
            }
            return TRUE;
        }
        default:
            return TRUE;
        }
    }

    // NOTE: Do not remove this; Detours needs at least 1 export symbol to work
    __declspec(dllexport)
    uint32_t DollDllHelloWorld()
    {
        return 0x91779111;
    }
}

