#pragma once
#include <dolldbg/dolldbg.h>
#include <dolldbg/util/syserr.h>

namespace DollDbg
{
    namespace Util
    {
        // TLS but with minimal API usage and supports DLL usage, intended for LL hook stubs
        // .get(false) uses TlsGetValue(), which only calls other APIs on error (RtlNtStatusToDosError() and RtlSetLastWin32Error())

        class slimtls
        {
        protected:
            slimtls(slimtls&) = delete;

            DWORD _index;

        public:
            slimtls()
            {
                _index = TlsAlloc();
                if (_index == TLS_OUT_OF_INDEXES)
                    throw Util::make_syserr("slimtls()::(): TlsAlloc() failed");
            }

            slimtls(slimtls&& other) noexcept
            {
                *this = std::move(other);
            }

            ~slimtls()
            {
                if (_index != TLS_OUT_OF_INDEXES)
                    TlsFree(_index);

                _index = TLS_OUT_OF_INDEXES;
            }

            slimtls& operator=(slimtls&& other) noexcept
            {
                std::swap(_index, other._index);
                return *this;
            }

            uintptr_t get(bool checkZero = true) const
            {
                auto value = (uintptr_t)TlsGetValue(_index);
                if (value == 0 && checkZero)
                {
                    DWORD err = GetLastError();
                    if(err != ERROR_SUCCESS)
                        throw Util::make_syserr("slimtls()::(): TlsGetValue() failed", err);
                }
                return value;
            }

            void set(uintptr_t value) const
            {
                if(!TlsSetValue(_index, (LPVOID)value))
                    throw Util::make_syserr("slimtls()::(): TlsSetValue() failed");
            }
        };

        class slimtls_guard
        {
        protected:
            slimtls_guard(slimtls_guard&) = delete;
            slimtls_guard(slimtls_guard&&) = delete;

            const slimtls& _tls;

        public:
            slimtls_guard(const slimtls& tls, uintptr_t value = 1)
                : _tls(tls) { _tls.set(value); }
            ~slimtls_guard()
                { _tls.set(0); }
        };
    }
}