#include "victim.h"

#include <dolldbg/rpc/npipe.h>
#include <dolldbg/util/exc.h>
#include <dolldbg/util/syserr.h>
#include <dolldbg/util/slimevent.h>
#include <dolldbg/protocol/protocol.h>
#include <dolldbg/protocol/victim.h>
#include <dolldbg/protocol/server.v.h>

#include <detours.h>

namespace DollDbg
{
    Victim::Victim* Victim::Victim::_instance = nullptr;

    namespace Victim
    {
        DWORD WINAPI Victim::_unloadThreadProc(LPVOID lpParameter)
        {
            try
            {
                Victim::fini();
            }
            catch (...)
            {
                OutputDebugString(Util::string_from_exc().c_str());
            }

            FreeLibraryAndExitThread((HMODULE)lpParameter, 0);
            return 42;
        }

        // ---

        Victim::Victim(HMODULE hModule, BOOL isStatic)
            : _hModule(hModule), _hit(true), _hitStop(false), _initDynEvt(0) // NOTE: _hit(true) to make sure no hooks are triggered before WaitForHook()
        {
            // RWX heap for LL hook stubs
            _execHeap = HeapCreate(HEAP_CREATE_ENABLE_EXECUTE, 0, 0);
            if (!_execHeap)
                throw Util::make_syserr("Victim::(): HeapCreate() failed");

            // Extract npipe path from detours payload
            DWORD payloadLen = 0;
            TCHAR* payload = (TCHAR*)DetourFindPayloadEx(Protocol::PayloadGuid::VictimTransport, &payloadLen);
            if (!payload || payloadLen == 0)
                throw Util::make_syserr("Victim::(): DetourFindPayloadEx() failed");

            // Register RPC channels
            _rpc.registerChannel(
                (Rpc::channel_t)::P::Victim::RpcChannel::Hook,
                [this](const buffer_t& req) { return _onRpcHook(req); });
            _rpc.registerChannel(
                (Rpc::channel_t)::P::Victim::RpcChannel::Unhook,
                [this](const buffer_t& req) { return _onRpcUnhook(req); });
            _rpc.registerChannel(
                (Rpc::channel_t)::P::Victim::RpcChannel::WaitForHook,
                [this](const buffer_t& req) { return _onRpcWaitForHook(req); });
            _rpc.registerChannel(
                (Rpc::channel_t)::P::Victim::RpcChannel::Detach,
                [this](const buffer_t& req) { return _onRpcDetach(req); });
            _rpc.registerChannel(
                (Rpc::channel_t)::P::Victim::RpcChannel::LoadLib,
                [this](const buffer_t& req) { return _onRpcLoadLib(req); });
            
            // Fail-safe callback
            _rpc.onException([this](auto exc) { return _onRpcExc(exc); });

            if (isStatic)
                _hookEp();

            std::thread([this, payload, isStatic]()
                {
                    // Establish connection
                    _rpc.ready(Rpc::NPipeClientTransportFactory(string_t(payload)));
            
                    // Online packet
                    _sendOnline();

                    if (!isStatic)
                    {
                        hookHLOnHook(nullptr, &_initDynEvt, false);
                        Util::slimevent(&_initDynEvt).notify();
                    }
                }).detach();
        }

        Victim::~Victim()
        {
            Util::slimtls_guard tlsguard(_tls);

            // Begin rundown
            _hitStop = true;
            _hitCv.notify_all();

            // Offline packet
            try
            {
                _rpc.call(
                    (Rpc::channel_t)::P::Server::V::RpcChannel::Offline,
                    Protocol::Serialize<::P::Void>(::P::CreateVoid));
            }
            catch (...)
            {
                // Empty
            }
            
            // Uninstall all hooks
            std::lock_guard<std::mutex> lck(_hooksMtx);
            std::set<void*> vas;
            for (const auto& hook : _hooks)
                vas.insert(hook.first);
            for (auto va : vas)
                _unhook(va);

            if(_execHeap)
                HeapDestroy(_execHeap);
            _execHeap = NULL;
        }
    }
}