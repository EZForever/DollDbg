#pragma once
#include <dolldbg/dolldbg.h>

#include <iostream>

namespace DollDbg
{
    namespace Util
    {
#ifdef UNICODE
        using tistream = std::wistream;
        using tostream = std::wostream;

#define tcin wcin
#define tcout wcout
#define tcerr wcerr
#else
        using tistream = std::istream;
        using tostream = std::ostream;

#define tcin cin
#define tcout cout
#define tcerr cerr
#endif
    }
}