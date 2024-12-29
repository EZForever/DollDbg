#pragma once
#include <dolldbg/dolldbg.h>

#include <cctype>
#include <cwctype>
#include <algorithm>

namespace DollDbg
{
    namespace Util
    {
#ifdef UNICODE
        inline string_t::value_type totlower(string_t::value_type ch)
            { return towlower(ch); }
#else
        inline string_t::value_type totlower(string_t::value_type ch)
            { return tolower(ch); }
#endif

        inline string_t string_tolower(const string_t& str)
        {
            string_t result;
            result.resize(str.size());
            std::transform(str.cbegin(), str.cend(), result.begin(), totlower);
            return result;
        }
    }
}