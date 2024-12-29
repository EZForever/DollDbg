#pragma once
#include <dolldbg/dolldbg.h>

#include <intrin.h>

namespace DollDbg
{
    namespace Util
    {
        // Intrinsic level spin lock wait on a 32-bit memory location utilizing x86's atomicity guarantees
        // Intended for LL hook stubs since they need a non-context-breaking event mechanism

        class slimevent
        {
        protected:
            slimevent(slimevent&) = delete;

            volatile uint32_t* _state;
            uint32_t _initial;

        public:
            slimevent()
                : _state(nullptr), _initial(0)
            {
                // Empty
            }

            slimevent(volatile uint32_t* state)
                : _state(state), _initial(_InterlockedCompareExchange(state, 0, 0))
            {
                // Atomicity is only guaranteed if the operand is 4-byte aligned
                // This is almost always the case for stack varibles (which we're using)
                // For any other cases the spin won't work and may cause data corruption so be dragons
                if (((uintptr_t)state & 0b11) != 0)
                    __fastfail(FAST_FAIL_INVALID_ARG);
            }

            slimevent(slimevent&& other) noexcept
                : slimevent()
            {
                *this = std::move(other);
            }

            slimevent& operator=(slimevent&& other) noexcept
            {
                std::swap(_state, other._state);
                std::swap(_initial, other._initial);
                return *this;
            }

            void wait() const
            {
                while(_InterlockedExchange(_state, _initial) == _initial)
                    YieldProcessor();
            }

            void notify() const
            {
                while(_InterlockedExchange(_state, ~_initial) != _initial)
                    YieldProcessor();
            }
        };
    }
}