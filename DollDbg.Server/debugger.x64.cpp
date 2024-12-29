#include "server.h"

namespace DollDbg
{
    namespace Server
    {
        enum class _reg_t : ULONG
        {
            rax = 0,
            rsp = 4,
            rip = 16
        };

        inline DEBUG_VALUE _toDebugValue(uintptr_t value)
        {
            DEBUG_VALUE result { 0 };
            result.I64 = value;
            result.Type = DEBUG_VALUE_INT64;
            return result;
        }

        void Server::_manipGo(victim_t& victim, uintptr_t hook, const hook_t& hookData)
        {
            DEBUG_VALUE value;
            HRESULT hr;
            
            // RIP = trampoline
            value = _toDebugValue((uintptr_t)hookData.trampoline);
            hr = victim.registers->SetValue((ULONG)_reg_t::rip, &value);
            if(FAILED(hr))
                throw Util::make_syserr("Server::_manipGo(): IDebugRegisters::SetValue() failed", hr);
        }

        void Server::_manipBreakOnReturn(victim_t& victim, uintptr_t hook, const hook_t& hookData)
        {
            DEBUG_VALUE value;
            HRESULT hr;
            
            // RSP -= 8
            hr = victim.registers->GetValue((ULONG)_reg_t::rsp, &value);
            if (FAILED(hr))
                throw Util::make_syserr("Server::_manipBreakOnReturn(): IDebugRegisters::GetValue() failed", hr);

            value.I64 -= 8;
            hr = victim.registers->SetValue((ULONG)_reg_t::rsp, &value);
            if (FAILED(hr))
                throw Util::make_syserr("Server::_manipBreakOnReturn(): IDebugRegisters::SetValue() failed", hr);

            // [RSP] = pHook
            hr = victim.dataSpaces->WriteVirtual(value.I64, &hook, sizeof(uintptr_t), NULL);
            if (FAILED(hr))
                throw Util::make_syserr("Server::_manipBreakOnReturn(): IDebugDataSpaces::WriteVirtual() failed", hr);

            // RIP = HookLLReturn
            value = _toDebugValue(victim.return_stub);
            hr = victim.registers->SetValue((ULONG)_reg_t::rip, &value);
            if (FAILED(hr))
                throw Util::make_syserr("Server::_manipBreakOnReturn(): IDebugRegisters::SetValue() failed", hr);
        }

        void Server::_manipSkip(victim_t& victim, uintptr_t hook, const hook_t& hookData, const ::P::UInt64* returnValue, const ::P::UInt64* stackBytes)
        {
            DEBUG_VALUE value;
            HRESULT hr;

            if (returnValue)
            {
                // RAX = returnValue
                value = _toDebugValue((uintptr_t)returnValue->v());
                hr = victim.registers->SetValue((ULONG)_reg_t::rax, &value);
                if (FAILED(hr))
                    throw Util::make_syserr("Server::_manipSkip(): IDebugRegisters::SetValue() failed", hr);
            }

            // RIP = [RSP]
            hr = victim.registers->GetValue((ULONG)_reg_t::rsp, &value);
            if (FAILED(hr))
                throw Util::make_syserr("Server::_manipSkip(): IDebugRegisters::GetValue() failed", hr);
            uintptr_t rsp = value.I64;

            uintptr_t return_addr = 0;
            hr = victim.dataSpaces->ReadVirtual(value.I64, &return_addr, 8, NULL);
            if (FAILED(hr))
                throw Util::make_syserr("Server::_manipSkip(): IDebugDataSpaces::WriteVirtual() failed", hr);

            value = _toDebugValue(return_addr);
            hr = victim.registers->SetValue((ULONG)_reg_t::rip, &value);
            if (FAILED(hr))
                throw Util::make_syserr("Server::_manipSkip(): IDebugRegisters::SetValue() failed", hr);

            // RSP += 8 + stackBytes
            value = _toDebugValue(rsp + 8 + (stackBytes ? (uintptr_t)stackBytes->v() : 0));
            hr = victim.registers->SetValue((ULONG)_reg_t::rsp, &value);
            if (FAILED(hr))
                throw Util::make_syserr("Server::_manipSkip(): IDebugRegisters::SetValue() failed", hr);
        }
    }
}