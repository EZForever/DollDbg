#pragma once
#include <cstdint>
#include <type_traits>

extern "C"
{
    // Low-level (assembly-facing) hook mechanics
    // Expect a lot of `uintptr_t`s

    // Type of code defined somewhere in memory that should not be executed directly
    using code_t = uint8_t[];

    // --- External declarations (NOTE: Keep sync with hookll.*.asm)

    extern const code_t HookLLCallStub;
    extern const code_t HookLLReturnStub;

    extern const uintptr_t HookLLCallStub_oHookPtr;
    extern const uintptr_t HookLLCallStub_Size;

    // --- Public declarations

#include <pshpack4.h>
    struct hook_t
    {
        void* va;
        void* llstub;
        void* trampoline;
    };
    static_assert(std::is_standard_layout_v<hook_t>, "hook_t");
#include <poppack.h>

    uintptr_t HookLLOnHook(const hook_t *pHook, volatile uint32_t *pEvent, uintptr_t isReturning);
    void HookLLReturnStackPush(uintptr_t value);
    uintptr_t HookLLReturnStackPop();
}