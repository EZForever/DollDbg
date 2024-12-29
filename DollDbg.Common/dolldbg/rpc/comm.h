#pragma once
#include <dolldbg/rpc/rpc.h>
#include <dolldbg/rpc/overlapped.h>

namespace DollDbg
{
    namespace Rpc
    {
        class CommTransport : public OverlappedTransport
        {
        public:
            CommTransport(HANDLE handle)
                : OverlappedTransport(handle) {}
            CommTransport(CommTransport&& other) noexcept
                : OverlappedTransport(std::move(other)) {}
            ~CommTransport();

            CommTransport& operator=(CommTransport&& other) noexcept
                { OverlappedTransport::operator=(std::move(other)); return *this; }

            //void send(const buffer_t& buf) override;
            //buffer_t recv(size_t size) override;
            //void cancel() override;
        };

        class CommTransportFactory : public ITransportFactory<CommTransport>
        {
        protected:
            string_t _mode;

        public:
            CommTransportFactory(const string_t& mode);
            CommTransportFactory(CommTransportFactory&& other) noexcept
                { *this = std::move(other); }
            ~CommTransportFactory();

            CommTransportFactory& operator=(CommTransportFactory&& other) noexcept;

            CommTransport connect() override;
        };
    }
}