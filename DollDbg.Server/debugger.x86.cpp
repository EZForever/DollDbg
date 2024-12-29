#include "server.h"

namespace DollDbg
{
    namespace Server
    {
        enum class _reg_t : ULONG
        {
            eax = 9,
            esp = 14,
            eip = 11
        };

        inline DEBUG_VALUE _toDebugValue(uintptr_t value)
        {
            DEBUG_VALUE result{ 0 };
            result.I32 = value;
            result.Type = DEBUG_VALUE_INT32;
            return result;
        }

        void Server::_manipGo(victim_t& victim, uintptr_t hook, const hook_t& hookData)
        {
            DEBUG_VALUE value;
            HRESULT hr;

            // EIP = trampoline
            value = _toDebugValue((uintptr_t)hookData.trampoline);
            hr = victim.registers->SetValue((ULONG)_reg_t::eip, &value);
            if (FAILED(hr))
                throw Util::make_syserr("Server::_manipGo(): IDebugRegisters::SetValue() failed", hr);
        }

        void Server::_manipBreakOnReturn(victim_t& victim, uintptr_t hook, const hook_t& hookData)
        {
            DEBUG_VALUE value;
            HRESULT hr;

            // ESP -= 4
            hr = victim.registers->GetValue((ULONG)_reg_t::esp, &value);
            if (FAILED(hr))
                throw Util::make_syserr("Server::_manipBreakOnReturn(): IDebugRegisters::GetValue() failed", hr);

            value.I32 -= 4;
            hr = victim.registers->SetValue((ULONG)_reg_t::esp, &value);
            if (FAILED(hr))
                throw Util::make_syserr("Server::_manipBreakOnReturn(): IDebugRegisters::SetValue() failed", hr);

            // [ESP] = pHook
            hr = victim.dataSpaces->WriteVirtual(value.I32, &hook, sizeof(uintptr_t), NULL);
            if (FAILED(hr))
                throw Util::make_syserr("Server::_manipBreakOnReturn(): IDebugDataSpaces::WriteVirtual() failed", hr);

            // EIP = HookLLReturn
            value = _toDebugValue(victim.return_stub);
            hr = victim.registers->SetValue((ULONG)_reg_t::eip, &value);
            if (FAILED(hr))
                throw Util::make_syserr("Server::_manipBreakOnReturn(): IDebugRegisters::SetValue() failed", hr);
        }

        void Server::_manipSkip(victim_t& victim, uintptr_t hook, const hook_t& hookData, const ::P::UInt64* returnValue, const ::P::UInt64* stackBytes)
        {
            DEBUG_VALUE value;
            HRESULT hr;

            if (returnValue)
            {
                // EAX = returnValue
                value = _toDebugValue((uintptr_t)returnValue->v());
                hr = victim.registers->SetValue((ULONG)_reg_t::eax, &value);
                if (FAILED(hr))
                    throw Util::make_syserr("Server::_manipSkip(): IDebugRegisters::SetValue() failed", hr);
            }

            // EIP = [ESP]
            hr = victim.registers->GetValue((ULONG)_reg_t::esp, &value);
            if (FAILED(hr))
                throw Util::make_syserr("Server::_manipSkip(): IDebugRegisters::GetValue() failed", hr);
            uintptr_t esp = value.I32;

            uintptr_t return_addr = 0;
            hr = victim.dataSpaces->ReadVirtual(value.I32, &return_addr, 4, NULL);
            if (FAILED(hr))
                throw Util::make_syserr("Server::_manipSkip(): IDebugDataSpaces::WriteVirtual() failed", hr);

            value = _toDebugValue(return_addr);
            hr = victim.registers->SetValue((ULONG)_reg_t::eip, &value);
            if (FAILED(hr))
                throw Util::make_syserr("Server::_manipSkip(): IDebugRegisters::SetValue() failed", hr);

            // RSP += 4 + stackBytes
            value = _toDebugValue(esp + 4 + (stackBytes ? (uintptr_t)stackBytes->v() : 0));
            hr = victim.registers->SetValue((ULONG)_reg_t::esp, &value);
            if (FAILED(hr))
                throw Util::make_syserr("Server::_manipSkip(): IDebugRegisters::SetValue() failed", hr);
        }
    }
}