#include "server.h"
#include <dolldbg/rpc/npipe.h>
#include <dolldbg/protocol/victim.h>
#include <dolldbg/protocol/server.v.h>

#include <detours.h>

namespace DollDbg
{
    namespace Server
    {
        std::tuple<std::shared_ptr<Rpc::Rpc>, string_t> Server::_newRpcV()
        {
            auto rpc = std::make_shared<Rpc::Rpc>();
            rpc->registerChannel(
                (Rpc::channel_t)::P::Server::V::RpcChannel::Online,
                [this, rpc](const buffer_t& req) { return _onRpcVOnline(rpc, req); });

            auto factory = Rpc::NPipeServerTransportFactory();
            string_t name(factory.name());

            // XXX: Condition racing here
            std::thread(
                [this, rpc, f = std::move(factory)]()
                    { rpc->ready(const_cast<Rpc::NPipeServerTransportFactory&>(f)); }).detach();

            return std::make_tuple(rpc, name);
        }

        void Server::_initRpcV(victim_t& victim, uint32_t pid)
        {
            victim.rpc->registerChannel(
                (Rpc::channel_t)::P::Server::V::RpcChannel::Offline,
                [this, pid](const buffer_t& req) { return _onRpcVOffline(pid, req); });
        }

        void Server::_finiRpcV(victim_t& victim)
        {
            victim.rpc.reset();
        }

        buffer_t Server::_onRpcVOnline(std::shared_ptr<Rpc::Rpc> rpc, const buffer_t& req)
        {
            auto onlineReq = Protocol::Unserialize<::P::Server::V::OnlineReq>(req);
            auto pid = onlineReq->proc()->pid();

            std::unique_lock<std::mutex> lck(_victimsMtx);
            try
            {
                _pendingVictims.erase(rpc);

                // NOTE: Implied default construction of victim_t
                auto& victim = _victims[pid];
                victim.name = Protocol::GetString(onlineReq->proc()->name());
                victim.return_stub = (uintptr_t)onlineReq->return_stub();
                victim.rpc = std::move(rpc);

                // Other per-victim initialization
                _initRpcV(victim, pid);
                _initDbg(victim, pid);

                _victimsCv.notify_all();
            }
            catch (...)
            {
                // XXX: Ugly
                _onRpcVOffline(pid, Protocol::Serialize<::P::Void>(::P::CreateVoid));
                throw;
            }

            return Protocol::Serialize<::P::Void>(::P::CreateVoid);
        }

        buffer_t Server::_onRpcVOffline(uint32_t pid, const buffer_t& req)
        {
            std::thread([this, pid]()
                {
                    std::lock_guard<std::mutex> lck(_victimsMtx);

                    // Per-victim finalization
                    auto& victim = _victims.at(pid);
                    victim.running = false;

                    _finiRpcV(victim);
                    _finiDbg(victim);

                    _victims.erase(pid);
                    _victimsCv.notify_all();
                }).detach();

            return Protocol::Serialize<::P::Void>(::P::CreateVoid);
        }

        void Server::_attachVictim(uint32_t pid)
        {
            std::unique_lock<std::mutex> lck(_victimsMtx);
            if (_victims.find(pid) != _victims.end())
                throw std::runtime_error("Server::_attachVictim(): Victim exists");

            HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
            if (!hProcess)
                throw Util::make_syserr("Server::_attachVictim(): OpenProcess() failed");
            
            HANDLE hThread = NULL;
            try
            {
                PVOID path = DetourCopyPayloadToProcessEx(
                    hProcess,
                    Protocol::PayloadGuid::VictimModulePath,
                    _victimModulePath.c_str(),
                    (DWORD)(sizeof(string_t::value_type) * (_victimModulePath.length() + 1))
                );
                if(!path)
                    throw Util::make_syserr("Server::_attachVictim(): DetourCopyPayloadToProcessEx() failed");

                hThread = CreateRemoteThread(
                    hProcess,
                    NULL, // lpThreadAttributes
                    0, // dwStackSize
                    (LPTHREAD_START_ROUTINE)LoadLibrary,
                    path,
                    CREATE_SUSPENDED,
                    NULL // lpThreadId
                );
                if(!hThread)
                    throw Util::make_syserr("Server::_attachVictim(): CreateRemoteThread() failed");
            }
            catch (...)
            {
                CloseHandle(hProcess);
                throw;
            }

            auto rpcParams = _newRpcV();
            auto& rpc = std::get<0>(rpcParams);
            auto& pipe = std::get<1>(rpcParams);
            try
            {
                if (!DetourCopyPayloadToProcess(
                    hProcess,
                    Protocol::PayloadGuid::VictimTransport,
                    pipe.c_str(),
                    (DWORD)(sizeof(string_t::value_type) * (pipe.length() + 1))
                ))
                    throw Util::make_syserr("Server::_attachVictim(): DetourCopyPayloadToProcess() failed");
            }
            catch (...)
            {
                CloseHandle(hProcess);
                CloseHandle(hThread);
                throw;
            }

            _pendingVictims.emplace(std::move(rpc));

            ResumeThread(hThread);

            CloseHandle(hProcess);
            CloseHandle(hThread);
        }

        uint32_t Server::_launchVictim(const string_t& cmd, const string_t& arg, const string_t& dir)
        {
            std::lock_guard<std::mutex> lck(_victimsMtx);

            STARTUPINFO si = { sizeof(STARTUPINFO) };
            PROCESS_INFORMATION pi = { 0 };
            std::vector<string_t::value_type> argBuf(arg.begin(), arg.end());
            argBuf.push_back(0);
            if (!DetourCreateProcessWithDllEx(
                cmd.empty() ? NULL : cmd.c_str(),
                arg.empty() ? NULL : argBuf.data(),
                NULL, // lpProcessAttributes
                NULL, // lpThreadAttributes
                FALSE, // bInheritHandles
                CREATE_SUSPENDED | CREATE_NEW_CONSOLE | CREATE_UNICODE_ENVIRONMENT,
                NULL, // lpEnvironment
                dir.empty() ? NULL : dir.c_str(),
                &si,
                &pi,
                string_to_mbcs(_victimModulePath).c_str(),
                NULL // pfCreateProcessW
            ))
                throw Util::make_syserr("Server::_launchVictim(): DetourCreateProcessWithDllEx() failed");

            auto rpcParams = _newRpcV();
            auto& rpc = std::get<0>(rpcParams);
            auto& pipe = std::get<1>(rpcParams);
            try
            {
                if (!DetourCopyPayloadToProcess(
                    pi.hProcess,
                    Protocol::PayloadGuid::VictimTransport,
                    pipe.c_str(),
                    (DWORD)(sizeof(string_t::value_type) * (pipe.length() + 1))
                ))
                    throw Util::make_syserr("Server::_launchVictim(): DetourCopyPayloadToProcess() failed");
            }
            catch (...)
            {
                TerminateProcess(pi.hProcess, STATUS_DLL_INIT_FAILED);
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
                throw;
            }

            _pendingVictims.emplace(std::move(rpc));

            ResumeThread(pi.hThread);

            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);

            return pi.dwProcessId;
        }

        void Server::_detachVictim(uint32_t pid)
        {
            std::lock_guard<std::mutex> lck(_victimsMtx);
            if (_victims.find(pid) == _victims.end())
                throw std::runtime_error("Server::_detachVictim(): Invalid victim pid");

            _victims[pid].rpc->call(
                (Rpc::channel_t)::P::Victim::RpcChannel::Detach,
                Protocol::Serialize<::P::Void>(::P::CreateVoid));
        }
    }
}