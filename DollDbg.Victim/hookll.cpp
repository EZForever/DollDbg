#include "hookll.h"

#include "victim.h"

using namespace DollDbg::Victim;

extern "C"
{
    uintptr_t HookLLOnHook(const hook_t* pHook, volatile uint32_t* pEvent, uintptr_t isReturning)
    {
        return (uintptr_t)Victim::get().hookHLOnHook(pHook, pEvent, (bool)isReturning);
    }

    void HookLLReturnStackPush(uintptr_t value)
    {
        Victim::get().hookHLReturnStackPush(value);
    }

    uintptr_t HookLLReturnStackPop()
    {
        return Victim::get().hookHLReturnStackPop();
    }
}