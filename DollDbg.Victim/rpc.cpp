#include "victim.h"

#include <dolldbg/util/exc.h>
#include <dolldbg/protocol/protocol.h>
#include <dolldbg/protocol/victim.h>
#include <dolldbg/protocol/server.v.h>

#include <psapi.h>

namespace DollDbg
{
    namespace Victim
    {
        void Victim::_sendOnline()
        {
            TCHAR baseNameBuf[MAX_PATH];
            DWORD baseNameLen = GetModuleBaseName(GetCurrentProcess(), NULL, baseNameBuf, MAX_PATH);
            if (baseNameLen == 0)
                throw Util::make_syserr("Victim::_sendOnline(): GetModuleBaseName() failed");
            string_t baseName(baseNameBuf, baseNameBuf + baseNameLen);

            buffer_t req = Protocol::Serialize<::P::Server::V::OnlineReq>([&baseName](auto& b)
                {
                    return ::P::Server::V::CreateOnlineReq(b,
                        ::P::CreateProc(b,
                            GetCurrentProcessId(),
                            Protocol::CreateString(b, baseName)),
                        (uint64_t)(uintptr_t)HookLLReturnStub);
                });
            _rpc.call((Rpc::channel_t)::P::Server::V::RpcChannel::Online, req);
        }

        void Victim::_onRpcExc(std::exception_ptr exc)
        {
            if (_hitStop)
                return;

            OutputDebugString(Util::string_from_exc(exc).c_str());
            _onRpcDetach(Protocol::Serialize<::P::Void>(::P::CreateVoid));
        }

        buffer_t Victim::_onRpcHook(const buffer_t& req)
        {
            auto hookReq = Protocol::Unserialize<::P::VirtualAddr>(req);
            void* va = (void*)(uintptr_t)hookReq->va();

            std::unique_lock<std::mutex> lck(_hooksMtx);
            _hook(va);

            return Protocol::Serialize<::P::Void>(::P::CreateVoid);
        }

        buffer_t Victim::_onRpcUnhook(const buffer_t& req)
        {
            auto unhookReq = Protocol::Unserialize<::P::VirtualAddr>(req);
            void* va = (void*)(uintptr_t)unhookReq->va();

            std::unique_lock<std::mutex> lck(_hooksMtx);
            _unhook(va);

            return Protocol::Serialize<::P::Void>(::P::CreateVoid);
        }

        buffer_t Victim::_onRpcWaitForHook(const buffer_t& req)
        {
            std::unique_lock<std::mutex> lck(_hitMtx);

            _hit = false;
            _hitCv.notify_one();
            _hitCv.wait(lck, [this]() { return _hit || _hitStop; });
            if (_hitStop)
                throw std::runtime_error("Victim::_onRpcWaitForHook(): _hitStop");

            buffer_t resp = Protocol::Serialize<::P::Victim::WaitForHookResp>([this](auto& b)
                {
                    return ::P::Victim::CreateWaitForHookResp(b,
                        (uint64_t)(uintptr_t)std::get<0>(_hitParams),
                        std::get<1>(_hitParams),
                        std::get<2>(_hitParams));
                });

            std::get<3>(_hitParams).wait();
            return resp;
        }

        buffer_t Victim::_onRpcDetach(const buffer_t& req)
        {
            HANDLE hThread = CreateThread(NULL, 0, _unloadThreadProc, (LPVOID)_hModule, 0, NULL);
            if (!hThread)
                throw Util::make_syserr("Victim::_onRpcDetach(): CreateThread() failed");

            CloseHandle(hThread);
            return Protocol::Serialize<::P::Void>(::P::CreateVoid);
        }

        buffer_t Victim::_onRpcLoadLib(const buffer_t& req)
        {
            auto loadLibReq = Protocol::Unserialize<::P::Victim::LoadLibReq>(req);
            HMODULE ret = LoadLibrary(Protocol::GetString(loadLibReq->path()).c_str());
            if(!ret)
                throw Util::make_syserr("Victim::_onRpcLoadLib(): LoadLibrary() failed");

            return Protocol::Serialize<::P::VirtualAddr>([this, ret](auto& b)
                {
                    return ::P::CreateVirtualAddr(b, (uint64_t)(uintptr_t)ret);
                });
        }
    }
}