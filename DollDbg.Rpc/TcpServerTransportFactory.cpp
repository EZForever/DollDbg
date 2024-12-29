#include "pch.h"
#include <dolldbg/rpc/tcp.h>

#include "tcp.h"

namespace DollDbg
{
    namespace Rpc
    {
        static constexpr int _TCP_BACKLOG_VALUE = 16;

        TcpServerTransportFactory::TcpServerTransportFactory(const string_t& host, const string_t& port)
            : _addr(NULL), _port(0), _listenSocket(INVALID_SOCKET)
        {
            _tcpInitializeAddr(_addr, host, port);

            int ret;
            SOCKET socket = ::socket(_addr->ai_family, _addr->ai_socktype, _addr->ai_protocol);
            if (socket == INVALID_SOCKET)
                throw RpcSystemException("TcpServerTransportFactory::connect(): socket() failed", WSAGetLastError());

            ret = ::bind(socket, _addr->ai_addr, (int)_addr->ai_addrlen);
            if (ret == SOCKET_ERROR)
            {
                ret = WSAGetLastError();
                closesocket(socket);
                throw RpcSystemException("TcpServerTransportFactory::connect(): bind() failed", ret);
            }

            sockaddr_storage bindAddr = { AF_UNSPEC };
            int bindAddrSize = sizeof(sockaddr_storage);
            ret = ::getsockname(socket, (sockaddr*)&bindAddr, &bindAddrSize);
            if (ret == SOCKET_ERROR)
            {
                ret = WSAGetLastError();
                closesocket(socket);
                throw RpcSystemException("TcpServerTransportFactory::connect(): getsockname() failed", ret);
            }
            _port = Util::ntohx(_addr->ai_family == AF_INET
                ? ((sockaddr_in*)&bindAddr)->sin_port
                : ((sockaddr_in6*)&bindAddr)->sin6_port);

            ret = ::listen(socket, _TCP_BACKLOG_VALUE);
            if (ret == SOCKET_ERROR)
            {
                ret = WSAGetLastError();
                closesocket(socket);
                throw RpcSystemException("TcpServerTransportFactory::connect(): listen() failed", ret);
            }
            _listenSocket = socket;
        }

        TcpServerTransportFactory::~TcpServerTransportFactory()
        {
            if (_addr)
                FreeAddrInfoEx(_addr);
            if (_listenSocket != INVALID_SOCKET)
                closesocket(_listenSocket);

            _addr = NULL;
            _listenSocket = INVALID_SOCKET;
        }

        TcpServerTransportFactory& TcpServerTransportFactory::operator=(TcpServerTransportFactory&& other) noexcept
        {
            std::swap(_addr, other._addr);
            std::swap(_port, other._port);
            std::swap(_listenSocket, other._listenSocket);
            return *this;
        }

        TcpTransport TcpServerTransportFactory::connect()
        {
            SOCKET socket = ::accept(_listenSocket, NULL, NULL);
            if(socket == INVALID_SOCKET)
                throw RpcSystemException("TcpClientTransportFactory::connect(): connect() failed", WSAGetLastError());

            return TcpTransport(socket);
        }

        uint16_t TcpServerTransportFactory::port()
        {
            if (_port == 0)
                throw std::runtime_error("TcpServerTransportFactory::port(): Not binded");

            return _port;
        }
    }
}