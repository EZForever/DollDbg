#pragma once
#include <winsock2.h>
#include <ws2tcpip.h>

namespace DollDbg
{
    namespace Rpc
    {
        class _TcpInitializer final
        {
        private:
            WSADATA wsaData;

            _TcpInitializer();
            ~_TcpInitializer();

        public:
            static const _TcpInitializer instance;
        };

        void _tcpInitializeAddr(PADDRINFOEX& addr, const string_t& host, const string_t& port);
    }
}