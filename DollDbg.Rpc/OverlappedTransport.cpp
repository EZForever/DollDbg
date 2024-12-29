#include "pch.h"
#include <dolldbg/rpc/overlapped.h>

namespace DollDbg
{
    namespace Rpc
    {
        OverlappedTransport::OverlappedTransport(HANDLE handle)
            : _handle(handle)
        {
            if (_handle == INVALID_HANDLE_VALUE)
                throw std::invalid_argument("OverlappedTransport::(): Invalid handle");
        }

        OverlappedTransport::~OverlappedTransport()
        {
            if (_handle != INVALID_HANDLE_VALUE)
                CloseHandle(_handle);

            _handle = INVALID_HANDLE_VALUE;
        }

        OverlappedTransport& OverlappedTransport::operator=(OverlappedTransport&& other) noexcept
        {
            std::swap(_handle, other._handle);
            std::swap(_overlappedRead, other._overlappedRead);
            std::swap(_overlappedWrite, other._overlappedWrite);
            return *this;
        }

        void OverlappedTransport::send(const buffer_t& buf)
        {
            size_t size = buf.size();
            size_t offset = 0;
            while (offset < size)
            {
                _overlappedWrite.setOffset(0);
                DWORD err = ERROR_SUCCESS;
                if (!WriteFile(_handle, buf.data() + offset, (DWORD)(size - offset), NULL, _overlappedWrite)
                    && (err = GetLastError()) != ERROR_IO_PENDING)
                {
                    if (err == ERROR_OPERATION_ABORTED)
                        throw RpcTransportException("OverlappedTransport::send(): Cancelled", error_t::transport_code_t::cancelled);
                    else
                        throw RpcSystemException("OverlappedTransport::send(): WriteFile() failed", err);
                }

                DWORD transferred;
                if (!GetOverlappedResult(_handle, _overlappedWrite, &transferred, TRUE))
                    throw RpcSystemException("OverlappedTransport::send(): GetOverlappedResult() failed", GetLastError());
                offset += transferred;
            }
        }

        buffer_t OverlappedTransport::recv(size_t size)
        {
            buffer_t buf(size);
            size_t offset = 0;
            while (offset < size)
            {
                _overlappedRead.setOffset(0);
                DWORD err = ERROR_SUCCESS;
                if (!ReadFile(_handle, buf.data() + offset, (DWORD)(size - offset), NULL, _overlappedRead)
                    && (err = GetLastError()) != ERROR_IO_PENDING)
                {
                    if (err == ERROR_OPERATION_ABORTED)
                        throw RpcTransportException("OverlappedTransport::recv(): Cancelled", error_t::transport_code_t::cancelled);
                    else
                        throw RpcSystemException("OverlappedTransport::recv(): ReadFile() failed", err);
                }

                DWORD transferred;
                if (!GetOverlappedResult(_handle, _overlappedRead, &transferred, TRUE))
                    throw RpcSystemException("OverlappedTransport::recv(): GetOverlappedResult() failed", GetLastError());
                offset += transferred;
            }
            return buf;
        }

        void OverlappedTransport::cancel()
        {
            if (_handle != INVALID_HANDLE_VALUE)
            {
                CancelIoEx(_handle, _overlappedRead);
                CancelIoEx(_handle, _overlappedWrite);
            }
        }
    }
}