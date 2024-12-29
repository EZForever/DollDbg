#include "pch.h"
#include <dolldbg/rpc/npipe.h>

namespace DollDbg
{
    namespace Rpc
    {
        NPipeTransport::~NPipeTransport()
        {
            if (_handle != INVALID_HANDLE_VALUE)
            {
                FlushFileBuffers(_handle);
                DisconnectNamedPipe(_handle); // XXX: This should not be done on client side
            }
        }
    }
}