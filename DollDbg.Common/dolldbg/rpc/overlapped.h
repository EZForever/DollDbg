#pragma once
#include <dolldbg/rpc/rpc.h>
#include <dolldbg/util/overlapped.h>

namespace DollDbg
{
    namespace Rpc
    {
        class OverlappedTransport : public ITransport
        {
        protected:
            HANDLE _handle;
            Util::overlapped_t _overlappedRead;
            Util::overlapped_t _overlappedWrite;

        public:
            OverlappedTransport(HANDLE _handle);
            OverlappedTransport(OverlappedTransport&& other) noexcept
                { *this = std::move(other); }
            ~OverlappedTransport();

            OverlappedTransport& operator=(OverlappedTransport&& other) noexcept;

            void send(const buffer_t& buf) override;
            buffer_t recv(size_t size) override;
            void cancel() override;
        };
    }
}