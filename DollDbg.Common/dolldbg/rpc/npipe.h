#pragma once
#include <dolldbg/rpc/overlapped.h>

#pragma comment(lib, "ole32.lib")

namespace DollDbg
{
    namespace Rpc
    {
        class NPipeTransport : public OverlappedTransport
        {
        public:
            NPipeTransport(HANDLE handle)
                : OverlappedTransport(handle) {}
            NPipeTransport(NPipeTransport&& other) noexcept
                : OverlappedTransport(std::move(other)) {}
            ~NPipeTransport();

            NPipeTransport& operator=(NPipeTransport&& other) noexcept
                { OverlappedTransport::operator=(std::move(other)); return *this; }

            //void send(const buffer_t& buf) override;
            //buffer_t recv(size_t size) override;
            //void cancel() override;
        };

        class NPipeClientTransportFactory : public ITransportFactory<NPipeTransport>
        {
        protected:
            string_t _name;

        public:
            NPipeClientTransportFactory(const string_t& name);
            NPipeClientTransportFactory(NPipeClientTransportFactory&& other) noexcept
                { *this = std::move(other); }
            ~NPipeClientTransportFactory();

            NPipeClientTransportFactory& operator=(NPipeClientTransportFactory&& other) noexcept;

            NPipeTransport connect() override;
        };

        class NPipeServerTransportFactory : public ITransportFactory<NPipeTransport>
        {
        protected:
            string_t _name;

        public:
            NPipeServerTransportFactory();
            NPipeServerTransportFactory(const string_t& name);
            NPipeServerTransportFactory(NPipeServerTransportFactory&& other) noexcept
                { *this = std::move(other); }
            ~NPipeServerTransportFactory();

            NPipeServerTransportFactory& operator=(NPipeServerTransportFactory&& other) noexcept;

            NPipeTransport connect() override;
            const string_t& name();
        };
    }
}