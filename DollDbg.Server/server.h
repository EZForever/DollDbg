#pragma once
#include <dolldbg/dolldbg.h>
#include <dolldbg/rpc/rpc.h>
#include <dolldbg/util/singleton.h>
#include <dolldbg/util/tiostream.h>
#include <dolldbg/protocol/protocol.h>

#include "xdbgeng.h"

#include <set>

namespace DollDbg
{
    namespace Server
    {
        // NOTE: Keep in sync with `hookll.h`
#include <pshpack4.h>
        struct hook_t
        {
            void* va;
            void* llstub;
            void* trampoline;
        };
        static_assert(std::is_standard_layout_v<hook_t>, "hook_t");
#include <poppack.h>

        struct victim_t
        {
            string_t name;
            uintptr_t return_stub = 0;
            bool running = true;
            bool inputting = false;

            std::shared_ptr<Rpc::Rpc> rpc;
            std::thread monitor;

            IDebugClient* client = nullptr;
            IDebugControl* control = nullptr;
            IDebugDataSpaces* dataSpaces = nullptr;
            IDebugRegisters* registers = nullptr;
            IDebugSystemObjects* systemObjects = nullptr;
            std::mutex engineMtx;

            std::set<uintptr_t> hooks;
            std::mutex hooksMtx;
        };

        class Server final : public Util::Singleton<Server>
        {
            friend class Util::Singleton<Server>;

        private:
            static constexpr TCHAR _victimModuleName[] = TEXT("DollDbg.Victim.dll");

            // ---

            Util::tostream& _out;
            string_t _connStr;
            string_t _victimModulePath;

            std::unique_ptr<Rpc::Rpc> _rpc;
            bool _running;

            std::map<uint32_t, victim_t> _victims;
            std::set<std::shared_ptr<Rpc::Rpc>> _pendingVictims;
            std::mutex _victimsMtx;
            std::condition_variable _victimsCv;

            Server(Util::tostream& out, const string_t& connStr);
            Server(Server&) = delete;
            Server(Server&&) = delete;
            ~Server();

            std::tuple<std::shared_ptr<Rpc::Rpc>, string_t> _newRpcV();
            void _initRpcV(victim_t& victim, uint32_t pid);
            void _finiRpcV(victim_t& victim);
            buffer_t _onRpcVOnline(std::shared_ptr<Rpc::Rpc> rpc, const buffer_t& req);
            buffer_t _onRpcVOffline(uint32_t pid, const buffer_t& req);
            void _attachVictim(uint32_t pid);
            uint32_t _launchVictim(const string_t& cmd, const string_t& arg, const string_t& dir);
            void _detachVictim(uint32_t pid);

            void _initDbg(victim_t& victim, uint32_t pid);
            void _finiDbg(victim_t& victim);
            void _monitor(victim_t& victim, uint32_t pid);
            void _updatePrompt(victim_t& victim);
            void _onDbgInput(victim_t& victim, bool inputting);
            void _onDbgOutput(uint32_t pid, ULONG mask, const string_t& message);

            void _manipGo(victim_t& victim, uintptr_t hook, const hook_t& hookData);
            void _manipBreakOnReturn(victim_t& victim, uintptr_t hook, const hook_t& hookData);
            void _manipSkip(victim_t& victim, uintptr_t hook, const hook_t& hookData, const ::P::UInt64* returnValue, const ::P::UInt64* stackBytes);

            void _initRpc();
            void _finiRpc();
            void _forProcesses(std::function<void(uint32_t, string_t)> action);
            void _onRpcExc(std::exception_ptr exc);
            buffer_t _onRpcOnline(const buffer_t& req);
            buffer_t _onRpcOffline(const buffer_t& req);
            buffer_t _onRpcAttach(const buffer_t& req);
            buffer_t _onRpcLaunch(const buffer_t& req);
            buffer_t _onRpcVictims(const buffer_t& req);
            buffer_t _onRpcDetach(const buffer_t& req);
            buffer_t _onRpcProcs(const buffer_t& req);
            buffer_t _onRpcKill(const buffer_t& req);
            buffer_t _onRpcBpSet(const buffer_t& req);
            buffer_t _onRpcBpList(const buffer_t& req);
            buffer_t _onRpcBpClear(const buffer_t& req);
            buffer_t _onRpcLoadLib(const buffer_t& req);
            buffer_t _onRpcInput(const buffer_t& req);
            buffer_t _onRpcEval(const buffer_t& req);

        public:
            void onCtrlBreak();
        };
    }
}