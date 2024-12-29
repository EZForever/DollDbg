#include "pch.h"
#include <dolldbg/rpc/tcp.h>

namespace DollDbg
{
    namespace Rpc
    {
        TcpTransport::TcpTransport(SOCKET socket)
            : _socket(socket)
        {
            if (socket == INVALID_SOCKET)
                throw std::invalid_argument("TcpTransport::(): Invalid socket");
        }

        TcpTransport::~TcpTransport()
        {
            if (_socket != INVALID_SOCKET)
            {
                shutdown(_socket, SD_BOTH);
                closesocket(_socket);
            }

            _socket = INVALID_SOCKET;
        }

        TcpTransport& TcpTransport::operator=(TcpTransport&& other) noexcept
        {
            std::swap(_socket, other._socket);
            return *this;
        }

        void TcpTransport::send(const buffer_t& buf)
        {
            size_t size = buf.size();
            size_t offset = 0;
            while (offset < size)
            {
                int ret = ::send(_socket, (char*)buf.data() + offset, (int)(size - offset), 0);
                if (ret == SOCKET_ERROR)
                {
                    int err = WSAGetLastError();
                    if(err == WSAEINTR)
                        throw RpcTransportException("TcpTransport::send(): Cancelled", error_t::transport_code_t::cancelled);
                    else
                        throw RpcSystemException("TcpTransport::send(): send() failed", err);
                }
                offset += ret;
            }
        }

        buffer_t TcpTransport::recv(size_t size)
        {
            buffer_t buf(size);
            size_t offset = 0;
            while (offset < size)
            {
                int ret = ::recv(_socket, (char*)buf.data() + offset, (int)(size - offset), 0);
                if (ret == SOCKET_ERROR)
                {
                    int err = WSAGetLastError();
                    if (err == WSAEINTR)
                        throw RpcTransportException("TcpTransport::recv(): Cancelled", error_t::transport_code_t::cancelled);
                    else
                        throw RpcSystemException("TcpTransport::recv(): recv() failed", err);
                }
                else if (ret == 0)
                {
                    throw RpcTransportException("TcpTransport::recv(): Connection closed", error_t::transport_code_t::connection_closed);
                }
                offset += ret;
            }
            return buf;
        }

        void TcpTransport::cancel()
        {
            // HACK: SOCKET is actually a HANDLE and can be cancelled like a file handle
            // https://stackoverflow.com/a/7714676
            // Causes WSAEINTR
            // XXX: There SHOULD be a way to cancel at least overlapped sockets

            if (_socket != INVALID_SOCKET)
                CancelIoEx((HANDLE)_socket, NULL);
        }
    }
}