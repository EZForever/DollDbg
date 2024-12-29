#include "pch.h"
#include <dolldbg/rpc/exc.h>
#include <dolldbg/rpc/tcp.h>

#include "tcp.h"

namespace DollDbg
{
    namespace Rpc
    {
        const _TcpInitializer _TcpInitializer::instance;

        _TcpInitializer::_TcpInitializer()
        {
            int ret = WSAStartup(MAKEWORD(2, 2), &wsaData);
            if (ret != 0)
                throw RpcSystemException("__TcpInitializer::(): WSAStartup() failed", WSAGetLastError());
        }

        _TcpInitializer::~_TcpInitializer()
        {
            WSACleanup();
        }

        void _tcpInitializeAddr(PADDRINFOEX& addr, const string_t& host, const string_t& port)
        {
            (void)_TcpInitializer::instance; // Ensure the initializer's presence

            ADDRINFOEX hints{ 0 };
            hints.ai_family = AF_UNSPEC;
            hints.ai_socktype = SOCK_STREAM;
            hints.ai_protocol = IPPROTO_TCP;

            int ret = GetAddrInfoEx(host.c_str(), port.c_str(), NS_ALL, NULL, &hints, &addr, NULL, NULL, NULL, NULL);
            if (ret != NO_ERROR)
                throw RpcSystemException("_tcpInitializeAddr(): GetAddrInfoEx() failed", ret);
            if (addr->ai_family != AF_INET && addr->ai_family != AF_INET6)
                throw std::runtime_error("_tcpInitializeAddr(): Unsupported address family");
        }
    }
}