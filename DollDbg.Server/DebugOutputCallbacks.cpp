#include "xdbgeng.h"

#include <intrin.h>

namespace DollDbg
{
    namespace Server
    {
        DebugOutputCallbacks::DebugOutputCallbacks(output_callback_t callback)
            : _callback(callback)
        {
            // Empty
        }

        STDMETHODIMP DebugOutputCallbacks::Output(ULONG Mask, PCTSTR Text)
        {
            _callback(Mask, string_t(Text));
            return S_OK;
        }
    }
}