#include "server.h"

#include <dolldbg/util/exc.h>
#include <dolldbg/protocol/client.h>
#include <dolldbg/protocol/victim.h>

namespace DollDbg
{
    namespace Server
    {
        void Server::_initDbg(victim_t& victim, uint32_t pid)
        {
            HRESULT hr = DebugCreate(__uuidof(IDebugClient), (PVOID*)&victim.client);
            if (FAILED(hr))
                throw Util::make_syserr("Server::_initDbg(): DebugCreate(IDebugClient) failed", hr);
#if 0 // XXX: Currently unused since non-invasive debugging does not trigger most debug events
            auto eventCallbacks = DebugEventCallbacks::Create();
            hr = victim.client->DBGENGT(SetEventCallbacks)(eventCallbacks);
            eventCallbacks->Release();
            if (FAILED(hr))
                throw Util::make_syserr("Server::_initDbg(): IDebugClient::SetEventCallbacks(DebugEventCallbacks) failed", hr);
#endif
            auto inputCallbacks = DebugInputCallbacks::Create([this, &victim](auto inputting) { _onDbgInput(victim, inputting); });
            hr = victim.client->SetInputCallbacks(inputCallbacks);
            inputCallbacks->Release();
            if (FAILED(hr))
                throw Util::make_syserr("Server::_initDbg(): IDebugClient::SetInputCallbacks(DebugInputCallbacks) failed", hr);

            auto outputCallbacks = DebugOutputCallbacks::Create([this, pid](auto mask, auto& message) { _onDbgOutput(pid, mask, message); });
            hr = victim.client->DBGENGT(SetOutputCallbacks)(outputCallbacks);
            outputCallbacks->Release();
            if (FAILED(hr))
                throw Util::make_syserr("Server::_initDbg(): IDebugClient::SetOutputCallbacks(DebugOutputCallbacks) failed", hr);

            // ---

            hr = victim.client->QueryInterface(__uuidof(IDebugControl), (PVOID*)&victim.control);
            if (FAILED(hr))
                throw Util::make_syserr("Server::_initDbg(): IDebugClient::QueryInterface(IDebugControl) failed", hr);

            hr = victim.client->QueryInterface(__uuidof(IDebugDataSpaces), (PVOID*)&victim.dataSpaces);
            if (FAILED(hr))
                throw Util::make_syserr("Server::_initDbg(): IDebugClient::QueryInterface(IDebugDataSpaces) failed", hr);

            hr = victim.client->QueryInterface(__uuidof(IDebugRegisters), (PVOID*)&victim.registers);
            if (FAILED(hr))
                throw Util::make_syserr("Server::_monitor(): IDebugClient::QueryInterface(IDebugRegisters) failed", hr);

            hr = victim.client->QueryInterface(__uuidof(IDebugSystemObjects), (PVOID*)&victim.systemObjects);
            if (FAILED(hr))
                throw Util::make_syserr("Server::_initDbg(): IDebugClient::QueryInterface(IDebugSystemObjects) failed", hr);

            // ---

            hr = victim.control->AddEngineOptions(
                DEBUG_ENGOPT_NO_EXECUTE_REPEAT // Prevent accidental command
                //| DEBUG_ENGOPT_DISALLOW_SHELL_COMMANDS // ditto (XXX: Necessary?)
                | DEBUG_ENGOPT_KD_QUIET_MODE // Supress extension messages (no extensions will be loaded anyway)
                | DEBUG_ENGOPT_DISABLE_MANAGED_SUPPORT // This is a low-level only usage of debugger engine (XXX: Necessary?)
                | DEBUG_ENGOPT_DISABLE_EXECUTION_COMMANDS // Prevent user exec (XXX: Necessary?)
                | DEBUG_ENGOPT_DISABLESQM // BIG BROTHER IS WATCHING YOU
            );
            if (FAILED(hr))
            {
                auto exc = Util::make_syserr("Server::_initDbg(): IDebugControl::AddEngineOptions() failed", hr);

                //throw exc; // XXX: dbgeng.dll from Debugging Tools for Windows 7 lacks support for multiple options
                _out << Util::string_from_exc(std::make_exception_ptr(exc)) << std::endl;
            }

            // ---

            // NOTE: Do not use DEBUG_ATTACH_NONINVASIVE_NO_SUSPEND here, since it sets 0x20 on a flag in ProcessInfo
            // Which in turn blocks LiveUserTargetInfo::SetTargetContextEx() with E_ACCESSDENIED, breaking context modification
            hr = victim.client->AttachProcess(0, pid, DEBUG_ATTACH_NONINVASIVE);
            if (FAILED(hr))
                throw Util::make_syserr("Server::_initDbg(): IDebugClient::AttachProcess() failed", hr);

            hr = victim.control->WaitForEvent(0, INFINITE);
            if (FAILED(hr))
                throw Util::make_syserr("Server::_initDbg(): IDebugControl::WaitForEvent() failed", hr);

            // TODO: Proper way to resume all threads
            hr = victim.control->DBGENGT(Execute)(DEBUG_OUTCTL_THIS_CLIENT, TEXT("~*m"), 0);
            if (FAILED(hr))
                throw Util::make_syserr("Server::_initDbg(): IDebugControl::Execute(\"~*m\") failed", hr);

            // ---

            victim.monitor = std::thread([this, &victim, pid]() { _monitor(victim, pid); });
        }

        void Server::_finiDbg(victim_t& victim)
        {
            if (victim.monitor.joinable())
                victim.monitor.join();

            std::lock_guard<std::mutex> lck(victim.engineMtx);
            if (victim.control)
                victim.control->Release();
            if (victim.dataSpaces)
                victim.dataSpaces->Release();
            if (victim.registers)
                victim.registers->Release();
            if (victim.systemObjects)
                victim.systemObjects->Release();
            if (victim.client)
            {
                victim.client->FlushCallbacks();
                #pragma warning(suppress: 6387) // Argument maybe NULL
                victim.client->DBGENGT(SetOutputCallbacks)(NULL);
                victim.client->EndSession(DEBUG_END_ACTIVE_DETACH);
                victim.client->Release();
            }
        }

        void Server::_monitor(victim_t& victim, uint32_t pid)
        {
            try
            {
                std::unique_lock<std::mutex> lck(victim.engineMtx, std::defer_lock);
                while (true)
                {
                    auto resp = victim.rpc->call(
                        (Rpc::channel_t)::P::Victim::RpcChannel::WaitForHook,
                        Protocol::Serialize<::P::Void>(::P::CreateVoid));
                    auto waitForHookResp = Protocol::Unserialize<::P::Victim::WaitForHookResp>(resp);

                    auto hook = (uintptr_t)waitForHookResp->hook();
                    auto tid = waitForHookResp->tid();
                    auto is_returning = waitForHookResp->is_returning();

                    HRESULT hr = S_OK;
                    lck.lock();

                    ULONG tidEngine = 0;
                    hr = victim.systemObjects->GetThreadIdBySystemId(tid, &tidEngine);
                    if (FAILED(hr))
                        throw Util::make_syserr("Server::_monitor(): IDebugSystemObjects::GetThreadIdBySystemId() failed", hr);

                    hr = victim.systemObjects->SetCurrentThreadId(tidEngine);
                    if (FAILED(hr))
                        throw Util::make_syserr("Server::_monitor(): IDebugSystemObjects::SetCurrentThreadId() failed", hr);

                    // TODO: Proper way to suspend the thread
                    hr = victim.control->DBGENGT(Execute)(DEBUG_OUTCTL_THIS_CLIENT, TEXT("~n"), 0);
                    if (FAILED(hr))
                        throw Util::make_syserr("Server::_monitor(): IDebugControl::Execute(\"~n\") failed", hr);

                    // Refresh thread state (for some reason `r` on a running thread totally breaks DbgEng's auto context refreshing)
                    // See below for more info on this command
                    hr = victim.control->DBGENGT(Execute)(DEBUG_OUTCTL_THIS_CLIENT, TEXT(".recxr -f"), 0);
                    if (FAILED(hr))
                        throw Util::make_syserr("Server::_monitor(): IDebugControl::Execute(\".recxr -f\") failed", hr);

                    hook_t hookData{ nullptr };
                    if (hook)
                    {
                        hr = victim.dataSpaces->ReadVirtual(hook, &hookData, sizeof(hook_t), NULL);
                        if (FAILED(hr))
                            throw Util::make_syserr("Server::_monitor(): IDebugDataSpaces::ReadVirtual() failed", hr);
                    }

                    lck.unlock();

                    uint64_t va = (uint64_t)(uintptr_t)hookData.va;
                    if (hookData.va)
                    {
                        // If VA is not found in the hooks list, this should be the gratuitous hook at the beginning of the attach
                        // And if VA is not NULL, we should unhook it
                        std::lock_guard<std::mutex> lck2(victim.hooksMtx);
                        if (victim.hooks.find((uintptr_t)hookData.va) == victim.hooks.end())
                        {
                            victim.rpc->call(
                                (Rpc::channel_t)::P::Victim::RpcChannel::Unhook,
                                Protocol::Serialize<::P::VirtualAddr>([&hookData](auto& b) { return ::P::CreateVirtualAddr(b, (uint64_t)hookData.va); }));

                            // By now the trampoline is gone and should be replaced by original VA
                            hookData.trampoline = hookData.va;

                            // Reset VA so not to confuse the client
                            va = 0;
                        }
                    }

                    // Notify & wait for result
                    resp = _rpc->call(
                        (Rpc::channel_t)::P::Client::RpcChannel::NotifyHook,
                        Protocol::Serialize<::P::Client::NotifyHookReq>([pid, tid, va, is_returning](auto& b)
                            {
                                return ::P::Client::CreateNotifyHookReq(b, pid, tid, va, is_returning);
                            }));
                    auto notifyHookResp = Protocol::Unserialize<::P::Client::NotifyHookResp>(resp);

                    lck.lock();

                    // Do this again since user may switch thread to elsewhere
                    hr = victim.systemObjects->SetCurrentThreadId(tidEngine);
                    if (FAILED(hr))
                        throw Util::make_syserr("Server::_monitor(): IDebugSystemObjects::SetCurrentThreadId() failed", hr);

                    // Manipulate thread context according to result
                    if (!hookData.va) // Dynamic initial hook does not respond to any action
                        __noop();
                    else if (is_returning) // Return hooks have an artificial context and must just `ret`
                        _manipSkip(victim, hook, hookData, nullptr, nullptr);
                    else if(notifyHookResp->action() == ::P::Client::HookAction::Skip)
                        _manipSkip(victim, hook, hookData, notifyHookResp->return_value(), notifyHookResp->stack_bytes());
                    else if (notifyHookResp->action() == ::P::Client::HookAction::Go)
                        _manipGo(victim, hook, hookData);
                    else if (notifyHookResp->action() == ::P::Client::HookAction::BreakOnReturn)
                        _manipBreakOnReturn(victim, hook, hookData);

                    // HACK: Undocumented & deprecated meta-command to flush modified context to thread
                    // NOTE: Impl. in dbgeng!DotReCxr()
                    // NOTE: Look for calls to TargetInfo::FlushRegContext() or TargetInfo::ChangeRegContext()
                    hr = victim.control->DBGENGT(Execute)(DEBUG_OUTCTL_THIS_CLIENT, TEXT(".recxr -f"), 0);
                    if (FAILED(hr))
                        throw Util::make_syserr("Server::_monitor(): IDebugControl::Execute(\".recxr -f\") failed", hr);

                    // TODO: Proper way to resume the thread
                    hr = victim.control->DBGENGT(Execute)(DEBUG_OUTCTL_THIS_CLIENT, TEXT("~m"), 0);
                    if (FAILED(hr))
                        throw Util::make_syserr("Server::_monitor(): IDebugControl::Execute(\"~m\") failed", hr);

                    lck.unlock();
                }
            }
            catch (...)
            {
                try
                {
                    std::lock_guard<std::mutex> lck(victim.engineMtx);

                    ULONG code = STILL_ACTIVE;
                    HRESULT hr = victim.client->GetExitCode(&code);
                    if (FAILED(hr))
                        throw Util::make_syserr("Server::_monitor(): IDebugClient::GetExitCode() failed", hr);

                    _rpc->call((Rpc::channel_t)::P::Client::RpcChannel::Offline,
                        Protocol::Serialize<::P::Client::OfflineReq>([pid, code](auto& b)
                            {
                                return ::P::Client::CreateOfflineReq(b, pid, code);
                            }));
                }
                catch (...)
                {
                    _out << Util::string_from_exc() << std::endl;
                }

                // XXX: Ugly
                if(victim.running)
                    _onRpcVOffline(pid, Protocol::Serialize<::P::Void>(::P::CreateVoid));
            }
        }

        void Server::_updatePrompt(victim_t& victim)
        {
            HRESULT hr = S_OK;
            if (victim.inputting)
            {
                hr = victim.control->DBGENGT(Output)(DEBUG_OUTPUT_PROMPT, TEXT("Input>"));
                if (FAILED(hr))
                    throw Util::make_syserr("Server::_onDbgInput(): IDebugControl::Output(...) failed", hr);
            }
            else if (victim.engineMtx.try_lock())
            {
                victim.engineMtx.unlock();

                hr = victim.control->DBGENGT(OutputPrompt)(DEBUG_OUTCTL_THIS_CLIENT, NULL);
                if (FAILED(hr))
                    throw Util::make_syserr("Server::_onDbgInput(): IDebugControl::OutputPrompt() failed", hr);
            }
            else
            {
                hr = victim.control->DBGENGT(Output)(DEBUG_OUTPUT_PROMPT, TEXT("*BUSY*"));
                if (FAILED(hr))
                    throw Util::make_syserr("Server::_onDbgInput(): IDebugControl::Output(...) failed", hr);
            }
        }

        void Server::_onDbgInput(victim_t& victim, bool inputting)
        {
            victim.inputting = inputting;
            _updatePrompt(victim);
        }

        void Server::_onDbgOutput(uint32_t pid, ULONG mask, const string_t& message)
        {
            if (!_rpc)
                return;

            auto type = ::P::Client::OutputType::MIN;
            switch (mask)
            {
            case DEBUG_OUTPUT_NORMAL:
            case DEBUG_OUTPUT_PROMPT_REGISTERS:
            case DEBUG_OUTPUT_SYMBOLS:
                type = ::P::Client::OutputType::Normal;
                break;
            case DEBUG_OUTPUT_ERROR:
                type = ::P::Client::OutputType::Error;
                break;
            case DEBUG_OUTPUT_WARNING:
            case DEBUG_OUTPUT_EXTENSION_WARNING:
                type = ::P::Client::OutputType::Warning;
                break;
            case DEBUG_OUTPUT_PROMPT:
                type = ::P::Client::OutputType::Prompt;
                break;
            case DEBUG_OUTPUT_DEBUGGEE:
                type = ::P::Client::OutputType::Debuggee;
                break;
            case DEBUG_OUTPUT_STATUS:
                type = ::P::Client::OutputType::Status;
                // NOTE: From a quick disassembly, this type of message only get fired on SymSrv/SrcSrv loading
                // NOTE: Impl. in dbgeng!UpdateStatusBar()
                break;
            case DEBUG_OUTPUT_VERBOSE:
                _out << message << std::flush;
                return;
            }

            try
            {
                _rpc->call((Rpc::channel_t)::P::Client::RpcChannel::Output,
                    Protocol::Serialize<::P::Client::OutputReq>([pid, type, &message](auto& b)
                        {
                            return ::P::Client::CreateOutputReq(b, pid, type, Protocol::CreateString(b, message));
                        }));
            }
            catch (...)
            {
                _out << Util::string_from_exc() << std::endl;
            }
        }
    }
}