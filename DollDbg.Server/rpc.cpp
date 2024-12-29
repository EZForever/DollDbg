#include "server.h"
#include "wow.h"

#include <dolldbg/rpc/tcp.h>
#include <dolldbg/rpc/comm.h>
#include <dolldbg/rpc/npipe.h>
#include <dolldbg/util/ctctype.h>
#include <dolldbg/protocol/client.h>
#include <dolldbg/protocol/victim.h>
#include <dolldbg/protocol/server.c.h>

#include <tlhelp32.h>

namespace DollDbg
{
    namespace Server
    {
        void Server::_initRpc()
        {
            _rpc = std::make_unique<Rpc::Rpc>();

            _rpc->registerChannel(
                (Rpc::channel_t)::P::Server::C::RpcChannel::Online,
                [this](const buffer_t& req) { return _onRpcOnline(req); });
            _rpc->registerChannel(
                (Rpc::channel_t)::P::Server::C::RpcChannel::Offline,
                [this](const buffer_t& req) { return _onRpcOffline(req); });
            _rpc->registerChannel(
                (Rpc::channel_t)::P::Server::C::RpcChannel::Attach,
                [this](const buffer_t& req) { return _onRpcAttach(req); });
            _rpc->registerChannel(
                (Rpc::channel_t)::P::Server::C::RpcChannel::Launch,
                [this](const buffer_t& req) { return _onRpcLaunch(req); });
            _rpc->registerChannel(
                (Rpc::channel_t)::P::Server::C::RpcChannel::Victims,
                [this](const buffer_t& req) { return _onRpcVictims(req); });
            _rpc->registerChannel(
                (Rpc::channel_t)::P::Server::C::RpcChannel::Detach,
                [this](const buffer_t& req) { return _onRpcDetach(req); });
            _rpc->registerChannel(
                (Rpc::channel_t)::P::Server::C::RpcChannel::Procs,
                [this](const buffer_t& req) { return _onRpcProcs(req); });
            _rpc->registerChannel(
                (Rpc::channel_t)::P::Server::C::RpcChannel::Kill,
                [this](const buffer_t& req) { return _onRpcKill(req); });
            _rpc->registerChannel(
                (Rpc::channel_t)::P::Server::C::RpcChannel::BpSet,
                [this](const buffer_t& req) { return _onRpcBpSet(req); });
            _rpc->registerChannel(
                (Rpc::channel_t)::P::Server::C::RpcChannel::BpList,
                [this](const buffer_t& req) { return _onRpcBpList(req); });
            _rpc->registerChannel(
                (Rpc::channel_t)::P::Server::C::RpcChannel::BpClear,
                [this](const buffer_t& req) { return _onRpcBpClear(req); });
            _rpc->registerChannel(
                (Rpc::channel_t)::P::Server::C::RpcChannel::LoadLib,
                [this](const buffer_t& req) { return _onRpcLoadLib(req); });
            _rpc->registerChannel(
                (Rpc::channel_t)::P::Server::C::RpcChannel::Input,
                [this](const buffer_t& req) { return _onRpcInput(req); });
            _rpc->registerChannel(
                (Rpc::channel_t)::P::Server::C::RpcChannel::Eval,
                [this](const buffer_t& req) { return _onRpcEval(req); });

            _rpc->onException([this](auto exc) { _onRpcExc(exc); });

            if (_connStr.rfind(TEXT("tcp:"), 0) == 0)
            {
                // tcp:<host>[:<port>]

                string_t host(_connStr, _connStr.find(TEXT(':')) + 1);
                string_t port;
                auto oPort = host.find_last_of(TEXT("]:"));
                if (oPort != string_t::npos && host[oPort] == TEXT(':'))
                {
                    port = string_t(host, oPort + 1);
                    host.erase(oPort);
                }

                //_out << TEXT("Trying on ") << host << TEXT(" port ") << port << std::endl;
                auto factory = Rpc::TcpServerTransportFactory(host, port);
                _out << TEXT("Listening on TCP ") << host << TEXT(" port ") << factory.port() << std::endl;
                _rpc->ready(factory);
            }
            else if (_connStr.rfind(TEXT("npipe:"), 0) == 0)
            {
                // npipe:\\.\pipe\<pipe_name>

                string_t pipe(_connStr, _connStr.find(TEXT(':')) + 1);
                _out << TEXT("Waiting on named pipe ") << pipe << std::endl;
                _rpc->ready(Rpc::NPipeServerTransportFactory(pipe));
            }
            else if (_connStr.rfind(TEXT("com"), 0) == 0)
            {
                // com<X>[:baud=<baud_rate>]

                string_t mode(_connStr);
                std::replace(mode.begin(), mode.end(), TEXT(','), TEXT(' '));

                //_out << TEXT("Trying on communication port ") << mode.substr(0, mode.find(TEXT(':'))) << std::endl;
                _rpc->ready(Rpc::CommTransportFactory(mode));
                _out << TEXT("Connected to communication port ") << mode.substr(0, mode.find(TEXT(':'))) << std::endl;
            }
            else
            {
                throw std::invalid_argument("Server::(): Invalid connection string");
            }

            _running = true;
        }

        void Server::_finiRpc()
        {
            if (!_running)
                return;
            _running = false;

            try
            {
                _rpc->call(
                    (Rpc::channel_t)::P::Client::RpcChannel::Shutdown,
                    Protocol::Serialize<::P::Void>(::P::CreateVoid));
            }
            catch (...)
            {
                // Empty
            }
            
            _rpc.reset();
        }
    
        void Server::_forProcesses(std::function<void(uint32_t, string_t)> action)
        {
            HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
            if (hSnapshot == INVALID_HANDLE_VALUE)
                throw Util::make_syserr("Server::_onRpcProcs(): CreateToolhelp32Snapshot() failed");

            PROCESSENTRY32 procEntry{ sizeof(PROCESSENTRY32) };
            if (!Process32First(hSnapshot, &procEntry))
            {
                DWORD err = GetLastError();
                CloseHandle(hSnapshot);
                throw Util::make_syserr("Server::_onRpcProcs(): Process32First() failed", err);
            }

            do {
                action(procEntry.th32ProcessID, string_t(procEntry.szExeFile));
            } while (Process32Next(hSnapshot, &procEntry));

            CloseHandle(hSnapshot);
        }

        void Server::_onRpcExc(std::exception_ptr exc)
        {
            if(_running)
                _onRpcOffline(Protocol::Serialize<::P::Void>(::P::CreateVoid));
        }

        buffer_t Server::_onRpcOnline(const buffer_t& req)
        {
            TCHAR name[MAX_COMPUTERNAME_LENGTH + 1] = { 0 };
            DWORD nameSize = _countof(name);
            if (!GetComputerName(name, &nameSize))
                throw Util::make_syserr("Server::_onRpcOnline(): GetComputerName() failed");

            return Protocol::Serialize<::P::Server::C::OnlineResp>([&name, nameSize](auto& b)
                {
                    return ::P::Server::C::CreateOnlineResp(b,
                        IMAGE_PROCESS_MACHINE,
                        Protocol::CreateString(b,
                            string_t(name, nameSize)));
                });
        }

        buffer_t Server::_onRpcOffline(const buffer_t& req)
        {
            if (!GenerateConsoleCtrlEvent(CTRL_C_EVENT, 0))
                throw Util::make_syserr("Server::_onRpcOffline(): GenerateConsoleCtrlEvent() failed");

            return Protocol::Serialize<::P::Void>(::P::CreateVoid);
        }

        buffer_t Server::_onRpcAttach(const buffer_t& req)
        {
            auto procId = Protocol::Unserialize<::P::ProcId>(req);
            auto pid = procId->pid();

            _attachVictim(pid);

            std::unique_lock<std::mutex> lck(_victimsMtx);
            _victimsCv.wait(lck, [this, pid]() { return _victims.find(pid) != _victims.end(); });

            return Protocol::Serialize<::P::Proc>([pid, &name = _victims.at(pid).name](auto& b)
            {
                return ::P::CreateProc(b, pid, Protocol::CreateString(b, name));
            });
        }

        buffer_t Server::_onRpcLaunch(const buffer_t& req)
        {
            auto launchReq = Protocol::Unserialize<::P::Server::C::LaunchReq>(req);
            auto cmd = Protocol::GetString(launchReq->cmd());
            auto arg = Protocol::GetString(launchReq->arg());
            auto dir = Protocol::GetString(launchReq->dir());

            auto pid = _launchVictim(cmd, arg, dir);

            std::unique_lock<std::mutex> lck(_victimsMtx);
            _victimsCv.wait(lck, [this, pid]() { return _victims.find(pid) != _victims.end(); });

            return Protocol::Serialize<::P::Proc>([pid, &name = _victims.at(pid).name](auto& b)
            {
                return ::P::CreateProc(b, pid, Protocol::CreateString(b, name));
            });
        }

        buffer_t Server::_onRpcVictims(const buffer_t& req)
        {
            std::lock_guard<std::mutex> lck(_victimsMtx);

            return Protocol::Serialize<::P::Server::C::VictimsResp>([this](auto& b)
                {
                    std::vector<flatbuffers::Offset<::P::Proc>> procs(_victims.size());
                    std::transform(_victims.cbegin(), _victims.cend(), procs.begin(), [&b](auto& victim)
                        {
                            return ::P::CreateProc(b,
                                victim.first,
                                Protocol::CreateString(b,
                                    victim.second.name));
                        });

                    return ::P::Server::C::CreateVictimsResp(b,
                        b.CreateVector(procs));
                });
        }

        buffer_t Server::_onRpcDetach(const buffer_t& req)
        {
            auto procId = Protocol::Unserialize<::P::ProcId>(req);
            auto pid = procId->pid();

            _detachVictim(pid);

            std::unique_lock<std::mutex> lck(_victimsMtx);
            _victimsCv.wait(lck, [this, pid]() { return _victims.find(pid) == _victims.end(); });

            return Protocol::Serialize<::P::Void>(::P::CreateVoid);
        }

        buffer_t Server::_onRpcProcs(const buffer_t& req)
        {
            auto procsReq = Protocol::Unserialize<::P::Server::C::ProcsReq>(req);
            auto prefix = Util::string_tolower(Protocol::GetString(procsReq->prefix()));

            std::vector<std::tuple<uint32_t, string_t>> entries;
            _forProcesses([&prefix, &entries](auto pid, auto name)
                {
                    if (Util::string_tolower(name).rfind(prefix, 0) != 0)
                        return;

                    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
                    if (!hProcess)
                        return;

                    if (xGetProcessMachine(hProcess) != IMAGE_PROCESS_MACHINE)
                    {
                        CloseHandle(hProcess);
                        return;
                    }

                    CloseHandle(hProcess);
                    entries.emplace_back(std::make_tuple(pid, name));
                });

            return Protocol::Serialize<::P::Server::C::ProcsResp>([&entries](auto& b)
                {
                    std::vector<flatbuffers::Offset<::P::Proc>> procs(entries.size());
                    std::transform(entries.cbegin(), entries.cend(), procs.begin(), [&b](auto& entry)
                        {
                            return ::P::CreateProc(b,
                                std::get<0>(entry),
                                Protocol::CreateString(b,
                                    std::get<1>(entry)));
                        });

                    return ::P::Server::C::CreateProcsResp(b,
                        b.CreateVector(procs));
                });
        }

        buffer_t Server::_onRpcKill(const buffer_t& req)
        {
            auto killReq = Protocol::Unserialize<::P::Server::C::KillReq>(req);
            auto pid = killReq->pid();
            
            if (pid)
            {
                HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
                if (!hProcess)
                    throw Util::make_syserr("Server::_onRpcKill(): OpenProcess() failed");

                if (!TerminateProcess(hProcess, STATUS_CONTROL_C_EXIT))
                {
                    DWORD err = GetLastError();
                    CloseHandle(hProcess);
                    throw Util::make_syserr("Server::_onRpcKill(): TerminateProcess() failed", err);
                }

                CloseHandle(hProcess);
            }
            else
            {
                auto target = Util::string_tolower(Protocol::GetString(killReq->name()));
                _forProcesses([&target](auto pid, auto name)
                    {
                        if (Util::string_tolower(name) != target)
                            return;

                        HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
                        if (!hProcess)
                            return;

                        if (!TerminateProcess(hProcess, STATUS_CONTROL_C_EXIT))
                        {
                            CloseHandle(hProcess);
                            return;
                        }

                        CloseHandle(hProcess);
                    });
            }

            return Protocol::Serialize<::P::Void>(::P::CreateVoid);
        }

        buffer_t Server::_onRpcBpSet(const buffer_t& req)
        {
            auto bpSetReq = Protocol::Unserialize<::P::Server::C::BpSetReq>(req);
            auto pid = bpSetReq->pid();
            auto va = bpSetReq->va();

            std::lock_guard<std::mutex> lck(_victimsMtx);
            auto& victim = _victims.at(pid);

            victim.rpc->call((Rpc::channel_t)::P::Victim::RpcChannel::Hook,
                Protocol::Serialize<::P::VirtualAddr>([va](auto& b)
                    {
                        return ::P::CreateVirtualAddr(b, va);
                    }));
            
            std::lock_guard<std::mutex> lck2(victim.hooksMtx);
            victim.hooks.emplace((uintptr_t)va);

            return Protocol::Serialize<::P::Void>(::P::CreateVoid);
        }

        buffer_t Server::_onRpcBpList(const buffer_t& req)
        {
            auto procId = Protocol::Unserialize<::P::ProcId>(req);
            auto pid = procId->pid();

            std::lock_guard<std::mutex> lck(_victimsMtx);
            auto& victim = _victims.at(pid);

            std::lock_guard<std::mutex> lck2(victim.hooksMtx);
            return Protocol::Serialize<::P::Server::C::BpListResp>([&hooks = victim.hooks](auto& b)
                {
                    std::vector<uint64_t> vas(hooks.size());
                    std::copy(hooks.cbegin(), hooks.cend(), vas.begin());

                    return ::P::Server::C::CreateBpListResp(b, b.CreateVector(vas));
                });
        }

        buffer_t Server::_onRpcBpClear(const buffer_t& req)
        {
            auto bpClearReq = Protocol::Unserialize<::P::Server::C::BpClearReq>(req);
            auto pid = bpClearReq->pid();
            auto va = bpClearReq->va();

            std::lock_guard<std::mutex> lck(_victimsMtx);
            auto& victim = _victims.at(pid);

            victim.rpc->call((Rpc::channel_t)::P::Victim::RpcChannel::Unhook,
                Protocol::Serialize<::P::VirtualAddr>([va](auto& b)
                    {
                        return ::P::CreateVirtualAddr(b,
                            va);
                    }));

            std::lock_guard<std::mutex> lck2(victim.hooksMtx);
            victim.hooks.erase((uintptr_t)va);

            return Protocol::Serialize<::P::Void>(::P::CreateVoid);
        }

        buffer_t Server::_onRpcLoadLib(const buffer_t& req)
        {
            auto loadLibReq = Protocol::Unserialize<::P::Server::C::LoadLibReq>(req);
            auto pid = loadLibReq->pid();
            auto path = Protocol::GetString(loadLibReq->path());

            std::lock_guard<std::mutex> lck(_victimsMtx);
            auto& victim = _victims.at(pid);

            return victim.rpc->call(
                (Rpc::channel_t)::P::Victim::RpcChannel::LoadLib,
                Protocol::Serialize<::P::Victim::LoadLibReq>([&path](auto& b)
                    {
                        return ::P::Victim::CreateLoadLibReq(b,
                            Protocol::CreateString(b, path));
                    }));
        }

        buffer_t Server::_onRpcInput(const buffer_t& req)
        {
            auto inputReq = Protocol::Unserialize<::P::Server::C::InputReq>(req);
            auto pid = inputReq->pid();
            auto line = Protocol::GetString(inputReq->line());

            std::unique_lock<std::mutex> lck(_victimsMtx);
            auto& victim = _victims.at(pid);
            lck.unlock();

            HRESULT hr = S_OK;
            
            if (victim.inputting)
            {
                hr = victim.control->DBGENGT(ReturnInput)(line.c_str());
                if (FAILED(hr))
                    throw Util::make_syserr("Server::_onRpcInput(): IDebugControl::ReturnInput(...) failed", hr);
            }
            else
            {
                std::unique_lock<std::mutex> lck2(victim.engineMtx);
                _updatePrompt(victim);

                hr = victim.control->DBGENGT(Execute)(DEBUG_OUTCTL_THIS_CLIENT, line.c_str(), 0);

                lck2.unlock();
                _updatePrompt(victim);

                if (FAILED(hr))
                    throw Util::make_syserr("Server::_onRpcInput(): IDebugControl::Execute(...) failed", hr);
            }

            return Protocol::Serialize<::P::Void>(::P::CreateVoid);
        }

        buffer_t Server::_onRpcEval(const buffer_t& req)
        {
            auto evalReq = Protocol::Unserialize<::P::Server::C::EvalReq>(req);
            auto pid = evalReq->pid();
            auto expr = Protocol::GetString(evalReq->expr());

            std::lock_guard<std::mutex> lck(_victimsMtx);
            auto& victim = _victims.at(pid);

            std::lock_guard<std::mutex> lck2(victim.engineMtx);
            DEBUG_VALUE value;
            HRESULT hr = victim.control->DBGENGT(Evaluate)(expr.c_str(), DEBUG_VALUE_INVALID, &value, NULL);
            if (FAILED(hr))
                throw Util::make_syserr("Server::_onRpcEval(): IDebugControl::Evaluate(...) failed", hr);

            return Protocol::Serialize<::P::Number>([&value](auto& b)
                {
                    ::P::NumberType number_type;
                    flatbuffers::Offset<void> number_value;
                    ::P::Float80Value f80Value;
                    ::P::UInt128Value uint128Value;
                    switch (value.Type)
                    {
                    case DEBUG_VALUE_INT8:
                        number_type = ::P::NumberType::I8;
                        number_value = ::P::CreateUInt8(b, value.I8).Union();
                        break;
                    case DEBUG_VALUE_INT16:
                        number_type = ::P::NumberType::I16;
                        number_value = ::P::CreateUInt16(b, value.I16).Union();
                        break;
                    case DEBUG_VALUE_INT32:
                        number_type = ::P::NumberType::I32;
                        number_value = ::P::CreateUInt32(b, value.I32).Union();
                        break;
                    case DEBUG_VALUE_INT64:
                        number_type = ::P::NumberType::I64;
                        number_value = ::P::CreateUInt64(b, value.I64).Union();
                        break;
                    case DEBUG_VALUE_FLOAT32:
                        number_type = ::P::NumberType::F32;
                        number_value = ::P::CreateFloat32(b, value.F32).Union();
                        break;
                    case DEBUG_VALUE_FLOAT64:
                        number_type = ::P::NumberType::F64;
                        number_value = ::P::CreateFloat64(b, value.F64).Union();
                        break;
                    case DEBUG_VALUE_FLOAT80:
                        f80Value = ::P::Float80Value(value.F80Bytes);
                        number_type = ::P::NumberType::F80;
                        number_value = ::P::CreateFloat80(b, &f80Value).Union();
                        break;
                    case DEBUG_VALUE_VECTOR64:
                        number_type = ::P::NumberType::V64;
                        number_value = ::P::CreateUInt64(b, value.VI64[0]).Union();
                        break;
                    case DEBUG_VALUE_VECTOR128:
                        uint128Value = ::P::UInt128Value(value.VI64);
                        number_type = ::P::NumberType::V128;
                        number_value = ::P::CreateUInt128(b, &uint128Value).Union();
                        break;
                    default:
                        throw std::runtime_error("Server::_onRpcEval(): Unknown result type");
                    }
                    return ::P::CreateNumber(b, number_type, number_value);
                });
        }
    }
}