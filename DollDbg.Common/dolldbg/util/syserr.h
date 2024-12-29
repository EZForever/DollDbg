#pragma once
#include <dolldbg/dolldbg.h>

#include <system_error>

namespace DollDbg
{
    namespace Util
    {
        inline std::system_error make_syserr(const char* what, DWORD err = GetLastError())
            { return std::system_error(std::error_code(err, std::system_category()), what); }

        // For functions which don't call SetLastError() (e.g. HeapAlloc())
        inline std::system_error make_syserr(const char* what, nullptr_t)
            { return make_syserr(what, 0x45504f4e /* NOPE */); }
    }
}