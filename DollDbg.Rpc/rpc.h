#pragma once
#include <dolldbg/rpc/rpc.h>

#include <cstdint>

namespace DollDbg
{
    namespace Rpc
    {
        using _length_t = uint32_t;

        constexpr _length_t LENGTH_ERROR = (_length_t)-1;

#include <pshpack1.h>
        struct _header_t
        {
        protected:
            channel_t _channel;
            response_t _response;
            _length_t _length;

        public:
            _header_t(channel_t channel, response_t response, _length_t length)
                : _channel(Util::htonx(channel)), _response(Util::htonx(response)), _length(Util::htonx(length)) {}

            channel_t channel() const
                { return Util::ntohx(_channel); }
            response_t response() const
                { return Util::ntohx(_response); }
            _length_t length() const
                { return Util::ntohx(_length); }
        };
#include <poppack.h>

        enum class Rpc::_dispatch_reason_t
        {
            none,
            request,
            incoming,
            response,
            exception
        };
    }
}