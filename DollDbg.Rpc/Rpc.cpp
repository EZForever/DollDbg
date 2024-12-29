#include "pch.h"
#include <dolldbg/rpc/rpc.h>
#include <dolldbg/util/finally.h>

#include "rpc.h"

namespace DollDbg
{
    namespace Rpc
    {
        Rpc::Rpc()
        {
            _lastResponse = 0;
            _reason = _dispatch_reason_t::none;
            _response = 0;
        }

        Rpc::~Rpc()
        {
            // This triggers RpcTransportException<connection_closed> to stop receiver
            // Receiver then triggers _dispatch_reason_t::exception to stop dispatcher
            if (_transport)
                _transport->cancel();

            if(_receiver.joinable())
                _receiver.join();
            if(_dispatcher.joinable())
                _dispatcher.join();
        }

        void Rpc::_ready(std::unique_ptr<ITransport> transport)
        {
            _transport = std::move(transport);

            _dispatcher = std::thread([this]()
                {
                    Util::slimtls_guard tlsguard(_tls);
                    _dispatch();
                });
            _receiver = std::thread([this]()
                {
                    Util::slimtls_guard tlsguard(_tls);
                    _receive();
                });
        }

        void Rpc::_dispatch()
        {
            std::unique_lock<std::mutex> lck(_dispatcherMtx);

            bool running = true;
            while (running)
            {
                _dispatcherCv.wait(lck, [this]() { return _reason != _dispatch_reason_t::none; });
                switch (_reason)
                {
                case _dispatch_reason_t::request:
                {
                    response_t response = _newResponse();
                    _requests.emplace(response, std::move(std::get<2>(_request)));
                    _sendBuffer(std::get<0>(_request), response, std::move(std::get<1>(_request)));
                    break;
                }
                case _dispatch_reason_t::incoming:
                {
                    _header_t* header = (_header_t*)std::get<0>(_incoming).data();
                    if (header->channel() == CHANNEL_RESPONSE)
                    {
                        std::promise<buffer_t> respPromise = std::move(_requests.at(header->response()));
                        _requests.erase(header->response());
                        if (header->length() == LENGTH_ERROR)
                            respPromise.set_exception(std::make_exception_ptr(RpcException("Rpc::_dispatch(): Incoming exception", *(error_t*)std::get<1>(_incoming).data())));
                        else
                            respPromise.set_value(std::move(std::get<1>(_incoming)));
                    }
                    else
                    {
                        std::lock_guard<std::mutex> lck(_channelsMtx);
                        const auto handlerIter = _channels.find(header->channel());
                        if (handlerIter == _channels.cend())
                        {
                            _sendException(CHANNEL_RESPONSE, header->response(), RpcTransportException("Rpc::_dispatch(): Unknown channel", error_t::transport_code_t::unknown_channel));
                        }
                        else
                        {
#if 0 // #ifdef _MSC_VER
                            // XXX: MSVC std::async() uses PPL which reuses threads
                            auto task = std::packaged_task<buffer_t()>([this, &handler = handlerIter->second, request = std::move(std::get<1>(_incoming)), response = header->response()]()
                                {
                                    Util::slimtls_guard tlsguard(_tls);
                                    return _work(handler, request, response);
                                });
                            _workers.emplace(header->response(), task.get_future());
                            std::thread(std::move(task)).detach();
#else
                            _workers.emplace(
                                header->response(),
                                std::async(std::launch::async, [this, &handler = handlerIter->second, request = std::move(std::get<1>(_incoming)), response = header->response()]()
                                    {
                                        Util::slimtls_guard tlsguard(_tls);
                                        return _work(handler, request, response);
                                    }));
#endif
                        }
                    }
                    break;
                }
                case _dispatch_reason_t::response:
                {
                    std::future<buffer_t> responseFuture = std::move(_workers.at(_response));
                    _workers.erase(_response);
                    try
                    {
                        _sendBuffer(CHANNEL_RESPONSE, _response, responseFuture.get());
                    }
                    catch (const RpcException& exc)
                    {
                        _sendException(CHANNEL_RESPONSE, _response, exc);
                    }
                    catch (const std::system_error& exc)
                    {
                        _sendException(CHANNEL_RESPONSE, _response, RpcSystemException("Rpc::_dispatch(): Converted system exception", exc.code().value()));
                    }
                    catch (...)
                    {
                        _sendException(CHANNEL_RESPONSE, _response, RpcApplicationException("Rpc::_dispatch(): Converted unknown exception", 0x45504f4e /* NOPE */));
                    }
                    break;
                }
                case _dispatch_reason_t::exception:
                {
                    // TODO: Report exceptions other than RpcTransportException<connection_closed>? (How?)
                    const RpcException& exc = RpcTransportException("Rpc::_dispatch(): Connection closed", error_t::transport_code_t::connection_closed);
                    
                    for (auto& request : _requests)
                        request.second.set_exception(std::make_exception_ptr(exc));
                    _requests.clear();

                    for (const auto& worker : _workers)
                    {
                        try
                        {
                            _sendException(CHANNEL_RESPONSE, worker.first, exc);
                        }
                        catch (...)
                        {
                            // Empty
                        }
                    }
                    _workers.clear();

                    running = false;
                    break;
                }
                }
                _reason = _dispatch_reason_t::none;
                _dispatcherCv.notify_one();
            }

            if (_exc_callback)
                _exc_callback(_exception);
        }

        void Rpc::_receive()
        {
            std::unique_lock<std::mutex> lck(_dispatcherMtx, std::defer_lock);

            try
            {
                while (true)
                {
                    buffer_t header = _transport->recv(sizeof(_header_t));
                    _length_t length = ((_header_t*)header.data())->length();
                    buffer_t buffer = _transport->recv(length == LENGTH_ERROR ? sizeof(error_t) : length);

                    lck.lock();
                    _dispatcherCv.wait(lck, [this]() { return _reason == _dispatch_reason_t::none; });

                    _incoming = std::make_tuple(std::move(header), std::move(buffer));

                    _reason = _dispatch_reason_t::incoming;
                    _dispatcherCv.notify_all();
                    lck.unlock();
                }
            }
            catch (...)
            {
                lck.lock();
                _dispatcherCv.wait(lck, [this]() { return _reason == _dispatch_reason_t::none; });

                _exception = std::current_exception();

                _reason = _dispatch_reason_t::exception;
                _dispatcherCv.notify_all();
                lck.unlock();
            }
        }

        buffer_t Rpc::_work(handler_t& handler, buffer_t request, response_t response)
        {
            Util::finally f(
                nullptr,
                [this, response]()
                {
                    std::unique_lock<std::mutex> lck(_dispatcherMtx);
                    _dispatcherCv.wait(lck, [this]() { return _reason == _dispatch_reason_t::none; });

                    _response = response;

                    _reason = _dispatch_reason_t::response;
                    _dispatcherCv.notify_all();
                });
            return handler(request);
        }

        void Rpc::_sendBuffer(channel_t channel, response_t response, const buffer_t& buffer)
        {
            buffer_t header(sizeof(_header_t));
            new (header.data()) _header_t(channel, response, (_length_t)buffer.size());
            try
            {
                _transport->send(header);
                _transport->send(buffer);
            }
            catch (...)
            {
                std::unique_lock<std::mutex> lck(_dispatcherMtx);
                _dispatcherCv.wait(lck, [this]() { return _reason == _dispatch_reason_t::none; });

                _exception = std::current_exception();

                _reason = _dispatch_reason_t::exception;
                _dispatcherCv.notify_all();
            }
        }

        void Rpc::_sendException(channel_t channel, response_t response, const RpcException& exc)
        {
            buffer_t header(sizeof(_header_t));
            buffer_t buffer(sizeof(error_t));
            new (header.data()) _header_t(channel, response, LENGTH_ERROR);
            new (buffer.data()) error_t(exc.error());
            try
            {
                _transport->send(header);
                _transport->send(buffer);
            }
            catch (...)
            {
                std::unique_lock<std::mutex> lck(_dispatcherMtx);
                _dispatcherCv.wait(lck, [this]() { return _reason == _dispatch_reason_t::none; });

                _exception = std::current_exception();

                _reason = _dispatch_reason_t::exception;
                _dispatcherCv.notify_all();
            }
        }

        response_t Rpc::_newResponse()
        {
            return ++_lastResponse;
        }

        buffer_t Rpc::call(channel_t channel, const buffer_t& req)
        {
            Util::slimtls_guard tlsguard(_tls);

            if (_exception)
                std::rethrow_exception(_exception);

            if (!_transport)
                throw std::runtime_error("Rpc::call(): Node not ready");

            if (channel == CHANNEL_RESPONSE)
                throw std::invalid_argument("Rpc::call(): Channel reserved for responses");

            std::promise<buffer_t> respPromise;
            std::future<buffer_t> respFuture = respPromise.get_future();

            {
                std::unique_lock<std::mutex> lck(_dispatcherMtx);
                _dispatcherCv.wait(lck, [this]() { return _reason == _dispatch_reason_t::none; });
            
                _request = std::make_tuple(channel, req, std::move(respPromise));

                _reason = _dispatch_reason_t::request;
                _dispatcherCv.notify_all();
            }

            return respFuture.get();
        }

        void Rpc::registerChannel(channel_t channel, handler_t handler)
        {
            Util::slimtls_guard tlsguard(_tls);

            if (channel == CHANNEL_RESPONSE)
                throw std::invalid_argument("Rpc::registerChannel(): Channel reserved for responses");

            std::lock_guard<std::mutex> lck(_channelsMtx);
            _channels[channel] = std::move(handler);
        }

        void Rpc::unregisterChannel(channel_t channel)
        {
            Util::slimtls_guard tlsguard(_tls);

            if (channel == CHANNEL_RESPONSE)
                throw std::invalid_argument("Rpc::unregisterChannel(): Channel reserved for responses");

            std::lock_guard<std::mutex> lck(_channelsMtx);
            _channels.erase(channel);
        }

        void Rpc::onException(exc_callback_t exc_callback)
        {
            Util::slimtls_guard tlsguard(_tls);

            _exc_callback = exc_callback;
        }

        bool Rpc::isRpcThread()
        {
            return _tls.get(false) != 0;
        }
    }
}