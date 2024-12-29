#pragma once
#include <dolldbg/dolldbg.h>
#include <dolldbg/rpc/rpc.h>
#include <dolldbg/util/slimtls.h>
#include <dolldbg/util/singleton.h>
#include <dolldbg/util/slimevent.h>

#include "hookll.h"

#include <mutex>
#include <condition_variable>

namespace DollDbg
{
    namespace Victim
    {
        class Victim final : public Util::Singleton<Victim>
        {
            friend class Util::Singleton<Victim>;

        private:
            static DWORD WINAPI _unloadThreadProc(LPVOID lpParameter);

            // ---

            Victim(HMODULE hModule, BOOL isStatic);
            Victim(Victim&) = delete;
            Victim(Victim&&) = delete;
            ~Victim();

            HMODULE _hModule;
            Rpc::Rpc _rpc;
            Util::slimtls _tls;
            Util::slimtls _tlsReturnStack;
            std::mutex _hitMtx;
            std::condition_variable _hitCv;
            bool _hit;
            bool _hitStop;
            std::tuple<const hook_t*, DWORD, bool, Util::slimevent> _hitParams;
            uint32_t _initDynEvt;

            HANDLE _execHeap;
            std::map<void*, hook_t*> _hooks;
            std::mutex _hooksMtx;

            void _sendOnline();
            void _onRpcExc(std::exception_ptr exc);
            buffer_t _onRpcHook(const buffer_t& req);
            buffer_t _onRpcUnhook(const buffer_t& req);
            buffer_t _onRpcWaitForHook(const buffer_t& req);
            buffer_t _onRpcDetach(const buffer_t& req);
            buffer_t _onRpcLoadLib(const buffer_t& req);

            void _detour(std::function<void()> operation);
            void _hook(void* va);
            void _unhook(void* va);
            void _hookEp();

        public:
            bool hookHLOnHook(const hook_t* pHook, volatile uint32_t* pEvent, bool isReturning);
            void hookHLReturnStackPush(uintptr_t value);
            uintptr_t hookHLReturnStackPop();

            void hookHLThreadFini();
        };
    }
}