#include "xdbgeng.h"

#include <intrin.h>

namespace DollDbg
{
    namespace Server
    {
        DebugInputCallbacks::DebugInputCallbacks(input_callback_t callback)
            : _callback(callback)
        {
            // Empty
        }

        STDMETHODIMP DebugInputCallbacks::StartInput(ULONG BufferSize)
        {
            _callback(true);
            return S_OK;
        }

        STDMETHODIMP DebugInputCallbacks::EndInput()
        {
            _callback(false);
            return S_OK;
        }
    }
}