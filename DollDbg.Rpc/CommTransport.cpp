#include "pch.h"
#include <dolldbg/rpc/comm.h>

namespace DollDbg
{
    namespace Rpc
    {
        CommTransport::~CommTransport()
        {
            if (_handle != INVALID_HANDLE_VALUE)
                FlushFileBuffers(_handle);
        }
    }
}