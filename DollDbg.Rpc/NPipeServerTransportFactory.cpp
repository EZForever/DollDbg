#include "pch.h"
#include <objbase.h>
#include <dolldbg/rpc/npipe.h>

namespace DollDbg
{
    namespace Rpc
    {
        // \\.\PIPE\DollDbg{00000000-0000-0000-0000-000000000000}
        static constexpr TCHAR NPIPE_PREFIX[] = TEXT("\\\\.\\PIPE\\DollDbg");

        NPipeServerTransportFactory::NPipeServerTransportFactory()
        {
            GUID guid;
            HRESULT hr = CoCreateGuid(&guid);
            if (FAILED(hr))
                throw RpcSystemException("NPipeServerTransportFactory::(): CoCreateGuid() failed", hr);

            OLECHAR guidChars[38 + 1] = { 0 };
            int size = StringFromGUID2(guid, (LPOLESTR)&guidChars, 38 + 1);
            if(size == 0)
                throw RpcSystemException("NPipeServerTransportFactory::(): StringFromGUID2() failed", -1);

            _name = NPIPE_PREFIX + string_from(guidChars);
        }

        NPipeServerTransportFactory::NPipeServerTransportFactory(const string_t& name)
            : _name(name)
        {
            // Empty
        }

        NPipeServerTransportFactory::~NPipeServerTransportFactory()
        {
            // Empty
        }

        NPipeServerTransportFactory& NPipeServerTransportFactory::operator=(NPipeServerTransportFactory&& other) noexcept
        {
            std::swap(_name, other._name);
            return *this;
        }

        NPipeTransport NPipeServerTransportFactory::connect()
        {
            HANDLE pipe = CreateNamedPipe(
                _name.c_str(),
                PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE | FILE_FLAG_OVERLAPPED,
                PIPE_REJECT_REMOTE_CLIENTS, // XXX: No SMB remote npipes
                1, // nMaxInstances
                0, // nOutBufferSize
                0, // nInBufferSize
                0, // nDefaultTimeOut
                NULL // lpSecurityAttributes // XXX: Set ACL for UWP full access?
            );
            if (pipe == INVALID_HANDLE_VALUE)
                throw RpcSystemException("NPipeServerTransportFactory::connect(): CreateNamedPipe() failed", GetLastError());

            Util::overlapped_t overlapped;
            if (!ConnectNamedPipe(pipe, overlapped))
            {
                DWORD err = GetLastError();
                if (err == ERROR_IO_PENDING)
                {
                    DWORD transferred; // Meaningless as per MSDN
                    if (!GetOverlappedResult(pipe, overlapped, &transferred, TRUE))
                    {
                        err = GetLastError();
                        CloseHandle(pipe);
                        throw RpcSystemException("NPipeServerTransportFactory::connect(): GetOverlappedResult() failed", err);
                    }
                }
                else if (err != ERROR_PIPE_CONNECTED)
                {
                    CloseHandle(pipe);
                    throw RpcSystemException("NPipeServerTransportFactory::connect(): ConnectNamedPipe() failed", err);
                }
            }

            return NPipeTransport(pipe);
        }

        const string_t& NPipeServerTransportFactory::name()
        {
            return _name;
        }
    }
}