#pragma once
#include <stdexcept>

#include <dolldbg/util/htonx.h>

namespace DollDbg
{
    namespace Rpc
    {
#include <pshpack1.h>
        struct error_t
        {
            using category_base_t = uint32_t;
            using code_t = uint32_t;

            enum class category_t : category_base_t
            {
                system = 'ESYS',
                transport = 'TRSP',
                application = 'EAPP'
            };

            enum transport_code_t : code_t
            {
                connection_closed = 'DEAD',
                cancelled = 'ABRT',
                unknown_channel = 'ECHN',
                invalid_payload = 'WHAT'
            };

        protected:
            category_base_t _category;
            code_t _code;

        public:
            error_t(category_t category, code_t code)
                : _category(Util::htonx((category_base_t)category)), _code(Util::htonx(code)) {}

            category_t category() const
                { return (category_t)Util::ntohx(_category); }
            code_t code() const
                { return Util::ntohx(_code); }
        };
#include <poppack.h>

        template<error_t::category_t...>
        class RpcBaseException;

        template<>
        class RpcBaseException<> : public std::runtime_error
        {
        protected:
            error_t _error;

        public:
            RpcBaseException(const char* what, const error_t& error)
                : std::runtime_error(what), _error(error) {}
            RpcBaseException(const char* what, error_t::category_t category, error_t::code_t code)
                : std::runtime_error(what), _error(category, code) {}

            const error_t& error() const
                { return _error; }
        };

        template<error_t::category_t category>
        class RpcBaseException<category> : public RpcBaseException<>
        {
        public:
            RpcBaseException(const char* what, error_t::code_t code)
                : RpcBaseException<>(what, category, code) {}
        };

        using RpcException = RpcBaseException<>;
        using RpcSystemException = RpcBaseException<error_t::category_t::system>;
        using RpcTransportException = RpcBaseException<error_t::category_t::transport>;
        using RpcApplicationException = RpcBaseException<error_t::category_t::application>;
    }
}