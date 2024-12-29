#pragma once
#include <winsock2.h>
#include <ws2tcpip.h>

#include <dolldbg/rpc/rpc.h>

#pragma comment(lib, "ws2_32.lib")

namespace DollDbg
{
    namespace Rpc
    {
        class TcpTransport : public ITransport
        {
        protected:
            SOCKET _socket;

        public:
            TcpTransport(SOCKET socket);
            TcpTransport(TcpTransport&& other) noexcept
                { *this = std::move(other); }
            ~TcpTransport();

            TcpTransport& operator=(TcpTransport&& other) noexcept;

            void send(const buffer_t& buf) override;
            buffer_t recv(size_t size) override;
            void cancel() override;
        };

        class TcpClientTransportFactory : public ITransportFactory<TcpTransport>
        {
        protected:
            PADDRINFOEX _addr;

        public:
            TcpClientTransportFactory(const string_t& host, const string_t& port);
            TcpClientTransportFactory(TcpClientTransportFactory&& other) noexcept
                { *this = std::move(other); }
            ~TcpClientTransportFactory();

            TcpClientTransportFactory& operator=(TcpClientTransportFactory&& other) noexcept;

            TcpTransport connect() override;
        };

        class TcpServerTransportFactory : public ITransportFactory<TcpTransport>
        {
        protected:
            PADDRINFOEX _addr;
            USHORT _port;
            SOCKET _listenSocket;

        public:
            TcpServerTransportFactory(const string_t& host, const string_t& port);
            TcpServerTransportFactory(TcpServerTransportFactory&& other) noexcept
                { *this = std::move(other); }
            ~TcpServerTransportFactory();

            TcpServerTransportFactory& operator=(TcpServerTransportFactory&& other) noexcept;

            TcpTransport connect() override;
            uint16_t port();
        };
    }
}