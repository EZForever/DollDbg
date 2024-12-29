#include "pch.h"
#include <dolldbg/rpc/comm.h>

namespace DollDbg
{
    namespace Rpc
    {
        CommTransportFactory::CommTransportFactory(const string_t& mode)
            : _mode(mode)
        {
            // Empty
        }

        CommTransportFactory::~CommTransportFactory()
        {
            // Empty
        }

        CommTransportFactory& CommTransportFactory::operator=(CommTransportFactory&& other) noexcept
        {
            std::swap(_mode, other._mode);
            return *this;
        }

        CommTransport CommTransportFactory::connect()
        {
            HANDLE port = CreateFile(
                _mode.substr(0, _mode.find_first_of(TEXT(": "))).c_str(),
                GENERIC_READ | GENERIC_WRITE,
                0, // dwShareMode
                NULL, // lpSecurityAttributes
                OPEN_EXISTING,
                FILE_FLAG_OVERLAPPED,
                NULL // hTemplateFile
            );
            if (port == INVALID_HANDLE_VALUE)
                throw RpcSystemException("CommTransportFactory::connect(): CreateFile() failed", GetLastError());

            DCB dcb;
            if (!GetCommState(port, &dcb))
            {
                DWORD err = GetLastError();
                CloseHandle(port);
                throw RpcSystemException("CommTransportFactory::connect(): GetCommState() failed", err);
            }

            COMMTIMEOUTS timeouts;
            if (!GetCommTimeouts(port, &timeouts))
            {
                DWORD err = GetLastError();
                CloseHandle(port);
                throw RpcSystemException("CommTransportFactory::connect(): GetCommTimeouts() failed", err);
            }

            if (!BuildCommDCBAndTimeouts(_mode.c_str(), &dcb, &timeouts))
            {
                DWORD err = GetLastError();
                CloseHandle(port);
                throw RpcSystemException("CommTransportFactory::connect(): BuildCommDCBAndTimeouts() failed", err);
            }

            if (!SetCommState(port, &dcb))
            {
                DWORD err = GetLastError();
                CloseHandle(port);
                throw RpcSystemException("CommTransportFactory::connect(): SetCommState() failed", err);
            }

            if (!SetCommTimeouts(port, &timeouts))
            {
                DWORD err = GetLastError();
                CloseHandle(port);
                throw RpcSystemException("CommTransportFactory::connect(): SetCommTimeouts() failed", err);
            }

            return CommTransport(port);
        }
    }
}