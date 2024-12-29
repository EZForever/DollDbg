#pragma once
#include <dolldbg/dolldbg.h>

#include <intrin.h>

namespace DollDbg
{
    namespace Util
    {
        // htonX() / ntohX() implementation that does not rely on winsock library
        // And often the impl can be faster (inlined intrinsics vs API calls (w/ bit shift for 16 bit integers))

        // NOTE: Windows only supports running on little-endian systems so no compatibility considerations
        // References:
        // x86 & x64 architectures supports little-endian only
        // ARM: https://docs.microsoft.com/en-us/cpp/build/overview-of-arm-abi-conventions#endianness
        // ARM64: https://docs.microsoft.com/en-us/cpp/build/arm64-windows-abi-conventions#endianness

        inline uint16_t htonx(uint16_t x)
            { return _byteswap_ushort(x); }
        inline uint32_t htonx(uint32_t x)
            { return _byteswap_ulong(x); }
        inline uint64_t htonx(uint64_t x)
            { return _byteswap_uint64(x); }

        inline uint16_t ntohx(uint16_t x)
            { return _byteswap_ushort(x); }
        inline uint32_t ntohx(uint32_t x)
            { return _byteswap_ulong(x); }
        inline uint64_t ntohx(uint64_t x)
            { return _byteswap_uint64(x); }
    }
}