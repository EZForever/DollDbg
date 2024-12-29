#include "pch.h"
#include <dolldbg/rpc/npipe.h>

namespace DollDbg
{
    namespace Rpc
    {
        NPipeClientTransportFactory::NPipeClientTransportFactory(const string_t& name)
            : _name(name)
        {
            // Empty
        }

        NPipeClientTransportFactory::~NPipeClientTransportFactory()
        {
            // Empty
        }

        NPipeClientTransportFactory& NPipeClientTransportFactory::operator=(NPipeClientTransportFactory&& other) noexcept
        {
            std::swap(_name, other._name);
            return *this;
        }

        NPipeTransport NPipeClientTransportFactory::connect()
        {
            if(!WaitNamedPipe(_name.c_str(), NMPWAIT_USE_DEFAULT_WAIT))
                throw RpcSystemException("NPipeClientTransportFactory::connect(): WaitNamedPipe() failed", GetLastError());

            HANDLE pipe = CreateFile(
                _name.c_str(),
                GENERIC_READ | GENERIC_WRITE,
                0, // dwShareMode
                NULL, // lpSecurityAttributes
                OPEN_EXISTING,
                FILE_FLAG_OVERLAPPED,
                NULL // hTemplateFile
            );
            if (pipe == INVALID_HANDLE_VALUE)
                throw RpcSystemException("NPipeClientTransportFactory::connect(): CreateFile() failed", GetLastError());

#if 0 // XXX: Message-based pipes does more trouble than good
            DWORD mode = PIPE_READMODE_MESSAGE;
            bool messageMode = true;
            if (!SetNamedPipeHandleState(pipe, &mode, NULL, NULL))
            {
                DWORD err = GetLastError();
                if (err == ERROR_INVALID_PARAMETER)
                {
                    // The server side is not opened in message mode (e.g. VirtualBox serial ports)
                    messageMode = false;
                }
                else
                {
                    CloseHandle(pipe);
                    throw RpcSystemException("NPipeClientTransportFactory::connect(): SetNamedPipeHandleState() failed", err);
                }
            }
#endif

            return NPipeTransport(pipe);
        }
    }
}