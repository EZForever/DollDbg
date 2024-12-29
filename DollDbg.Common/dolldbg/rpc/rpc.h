#pragma once
#include <map>
#include <mutex>
#include <future>
#include <memory>
#include <thread>
#include <type_traits>
#include <condition_variable>

#include <dolldbg/dolldbg.h>
#include <dolldbg/rpc/exc.h>
#include <dolldbg/util/slimtls.h>

namespace DollDbg
{
    namespace Rpc
    {
        using channel_t = uint16_t;
        using response_t = uint16_t;
        using handler_t = std::function<buffer_t(const buffer_t&)>;
        using exc_callback_t = std::function<void(std::exception_ptr)>;

        constexpr channel_t CHANNEL_RESPONSE = 0;

        class ITransport
        {
        protected:
            ITransport() = default;
            ITransport(ITransport&) = delete;

        public:
            virtual ~ITransport() = default;

            virtual void send(const buffer_t& buf) = 0;
            virtual buffer_t recv(size_t size) = 0;
            virtual void cancel() = 0;
        };

        template<class Ty> // where Ty : ITransport
        class ITransportFactory
        {
            static_assert(std::is_base_of<ITransport, Ty>::value, "ITransportFactory<Ty>");

        protected:
            ITransportFactory() = default;
            ITransportFactory(ITransportFactory&) = delete;

        public:
            using Product = Ty;

            virtual ~ITransportFactory() = default;

            virtual Product connect() = 0;
        };

        class Rpc
        {
            enum class _dispatch_reason_t;

        protected:
            Rpc(Rpc&) = delete;
            Rpc(Rpc&&) = delete;

            void _ready(std::unique_ptr<ITransport> transport);
            void _dispatch();
            void _receive();
            buffer_t _work(handler_t& handler, buffer_t request, response_t response);
            void _sendBuffer(channel_t channel, response_t response, const buffer_t& buffer);
            void _sendException(channel_t channel, response_t response, const RpcException& exc);
            response_t _newResponse();

            std::unique_ptr<ITransport> _transport;
            response_t _lastResponse;
            std::thread _dispatcher;
            std::thread _receiver;
            std::mutex _channelsMtx;
            std::map<channel_t, handler_t> _channels;
            std::map<response_t, std::promise<buffer_t>> _requests;
            std::map<response_t, std::future<buffer_t>> _workers;
            exc_callback_t _exc_callback;
            Util::slimtls _tls;

            std::once_flag _ready_flag;
            std::mutex _dispatcherMtx;
            std::condition_variable _dispatcherCv;
            _dispatch_reason_t _reason;
            std::tuple<channel_t, buffer_t, std::promise<buffer_t>> _request;
            std::tuple<buffer_t, buffer_t> _incoming;
            response_t _response;
            std::exception_ptr _exception;

        public:
            Rpc();
            ~Rpc();

            template<class Ty> // where Ty : ITransportFactory<?>
            void ready(Ty&& factory)
            {
                Util::slimtls_guard tlsguard(_tls);
                std::call_once(_ready_flag, [this, &factory]()
                    { _ready(std::make_unique<std::decay_t<Ty>::Product>(factory.connect())); });
            }

            buffer_t call(channel_t channel, const buffer_t& req);
            void registerChannel(channel_t channel, handler_t handler);
            void unregisterChannel(channel_t channel);
            void onException(exc_callback_t exc_callback = exc_callback_t());
            bool isRpcThread();
        };
    }
}