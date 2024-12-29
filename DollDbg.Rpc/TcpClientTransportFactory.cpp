#include "pch.h"
#include <dolldbg/rpc/tcp.h>

#include "tcp.h"

namespace DollDbg
{
    namespace Rpc
    {
        TcpClientTransportFactory::TcpClientTransportFactory(const string_t& host, const string_t& port)
            : _addr(NULL)
        {
            _tcpInitializeAddr(_addr, host, port);
        }

        TcpClientTransportFactory::~TcpClientTransportFactory()
        {
            if (_addr)
                FreeAddrInfoEx(_addr);

            _addr = NULL;
        }

        TcpClientTransportFactory& TcpClientTransportFactory::operator=(TcpClientTransportFactory&& other) noexcept
        {
            std::swap(_addr, other._addr);
            return *this;
        }

        TcpTransport TcpClientTransportFactory::connect()
        {
            SOCKET _socket = ::socket(_addr->ai_family, _addr->ai_socktype, _addr->ai_protocol);
            if (_socket == INVALID_SOCKET)
                throw RpcSystemException("TcpClientTransportFactory::connect(): socket() failed", WSAGetLastError());

            int ret = ::connect(_socket, _addr->ai_addr, (int)_addr->ai_addrlen);
            if (ret == SOCKET_ERROR)
            {
                ret = WSAGetLastError();
                closesocket(_socket);
                throw RpcSystemException("TcpClientTransportFactory::connect(): connect() failed", ret);
            }

            return TcpTransport(_socket);
        }
    }
}