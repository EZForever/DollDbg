#include "victim.h"
#include "xdetours.h"

#include <dolldbg/util/finally.h>

#include <psapi.h>
#include <malloc.h>
#include <dbghelp.h>
#include <tlhelp32.h>

#include <set>
#include <stack>

#include <detours.h>

namespace DollDbg
{
    namespace Victim
    {
        using return_stack_t = std::stack<uintptr_t>;

        void Victim::_detour(std::function<void()> operation)
        {
            // Pre-hold the process heap lock to prevent deadlock after DetourUpdateThread()
            // This is valid (maybe not fully supported) operation; from UCRT source comment (heapwalk.cpp function _heapwalk):
            // "... the caller must lock the heap for the duration of the heap walk, by calling the Windows API HeapLock."
            // XXX: This is only "safe enough" since current versions of MSVCRT/UCRT only uses the process heap for memory management
            Util::finally f(
                []()
                {
                    if (!HeapLock((HANDLE)_get_heap_handle()))
                        throw Util::make_syserr("Victim::_detour(): HeapLock() failed");
                },
                []()
                {
                    if (!HeapUnlock((HANDLE)_get_heap_handle()))
                        throw Util::make_syserr("Victim::_detour(): HeapUnlock() failed");
                });

            HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
            if (hSnapshot == INVALID_HANDLE_VALUE)
                throw Util::make_syserr("Victim::_detour(): CreateToolhelp32Snapshot() failed");

            std::set<HANDLE> threads;
            try
            {
                DWORD selfPid = GetCurrentProcessId();
                DWORD selfTid = GetCurrentThreadId();
                THREADENTRY32 entry = { sizeof(entry) };
                if (!Thread32First(hSnapshot, &entry))
                    throw Util::make_syserr("Victim::_detour(): Thread32First() failed");

                do {
                    if (entry.th32OwnerProcessID != selfPid || entry.th32ThreadID == selfTid)
                        continue;

                    HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, entry.th32ThreadID);
                    if(!hThread)
                        throw Util::make_syserr("Victim::_detour(): OpenThread() failed");

                    threads.insert(hThread);
                } while (Thread32Next(hSnapshot, &entry));
            }
            catch (...)
            {
                CloseHandle(hSnapshot);
                throw;
            }
            CloseHandle(hSnapshot);

            // Technically this could fail when another transaction is pending
            // However Victim::_hooks is synched so this will never happen
            DetourTransactionBegin();
            try
            {
                for (auto hThread : threads)
                {
                    LONG err = DetourUpdateThread(hThread);
                    if(err != NO_ERROR)
                        throw Util::make_syserr("Victim::_detour(): DetourUpdateThread() failed", err);
                }

                operation();
            }
            catch (...)
            {
                DetourTransactionAbort();
                for (auto hThread : threads)
                    CloseHandle(hThread);
                throw;
            }
            DetourTransactionCommit();
            for (auto hThread : threads)
                CloseHandle(hThread);
        }

        void Victim::_hook(void* va)
        {
            if (_hooks.find(va) != _hooks.end())
                throw std::runtime_error("Victim::_hook(): Hook exists");

            if (IsBadCodePtr((FARPROC)va))
                throw Util::make_syserr("Victim::_hook(): IsBadCodePtr() failed");

            auto hook = new hook_t;
            hook->va = va;
            hook->llstub = nullptr;
            hook->trampoline = xDetourCodeFromPointer(va, NULL);
            try
            {
                hook->llstub = HeapAlloc(_execHeap, 0, HookLLCallStub_Size);
                if (!hook->llstub)
                    throw Util::make_syserr("Victim::_hook(): HeapAlloc() failed", nullptr);

#ifdef _DEBUG // Debug builds puts a jump after the symbol
                memcpy(hook->llstub, xDetourCodeFromPointer((PVOID)HookLLCallStub, NULL), HookLLCallStub_Size);
#else
                memcpy(hook->llstub, HookLLCallStub, HookLLCallStub_Size);
#endif
                *(uintptr_t*)((uint8_t*)hook->llstub + HookLLCallStub_oHookPtr) = (uintptr_t)hook;

                _detour([hook]()
                    {
                        LONG ret = DetourAttach(&hook->trampoline, hook->llstub);
                        if(ret != NO_ERROR)
                            throw Util::make_syserr("Victim::_hook(): DetourAttach() failed", ret);
                    });
            }
            catch (...)
            {
                if (hook->llstub)
                    HeapFree(_execHeap, 0, hook->llstub);
                delete hook;
                throw;
            }
            _hooks.emplace(va, hook);
        }

        void Victim::_unhook(void* va)
        {
            auto hookEntry = _hooks.find(va);
            if (hookEntry == _hooks.end())
                throw std::runtime_error("Victim::_unhook(): Hook does not exist");

            auto hook = hookEntry->second;
            _detour([hook]()
                { DetourDetach(&hook->trampoline, hook->llstub); });

            _hooks.erase(hookEntry);
            HeapFree(_execHeap, 0, hook->llstub);
            delete hook;
        }

        void Victim::_hookEp()
        {
            HMODULE hModProcess = GetModuleHandle(NULL);
            if (!hModProcess)
                throw Util::make_syserr("Victim::_hookEp(): GetModuleHandle() failed");

            ULONG tlsDirSize = 0;
            auto tlsDir = (IMAGE_TLS_DIRECTORY*)ImageDirectoryEntryToDataEx(hModProcess, TRUE, IMAGE_DIRECTORY_ENTRY_TLS, &tlsDirSize, NULL);

            void* va = nullptr;
            if (tlsDir)
            {
                auto tlsCallbacks = (void**)tlsDir->AddressOfCallBacks;
                if (tlsCallbacks)
                    va = tlsCallbacks[0];
            }

            if (!va)
            {
#if 0 // XXX: This impl is more "flaky"
                auto ntHdr = ImageNtHeader(hModProcess);
                if(!ntHdr)
                    throw Util::make_syserr("Victim::_hookEp(): ImageNtHeader() failed");

                auto rva = ntHdr->OptionalHeader.AddressOfEntryPoint;
                va = ImageRvaToVa(ntHdr, hModProcess, rva, NULL);
                if(!va)
                    throw Util::make_syserr("Victim::_hookEp(): ImageRvaToVa() failed");
#else
                MODULEINFO mi;
                if(!GetModuleInformation(GetCurrentProcess(), hModProcess, &mi, sizeof(mi)))
                    throw Util::make_syserr("Victim::_hookEp(): GetModuleInformation() failed");
                va = mi.EntryPoint;
#endif
            }

            std::unique_lock<std::mutex> lck(_hooksMtx);
            _hook(va);
        }

        bool Victim::hookHLOnHook(const hook_t* hook, volatile uint32_t* pEvent, bool isReturning)
        {
            // Ignore internal threads
            if (_tls.get(false) != 0 || _rpc.isRpcThread())
                return false;

            Util::slimtls_guard tlsguard(_tls);
            Util::slimevent event(pEvent);
            std::unique_lock<std::mutex> lck(_hitMtx);

            _hitCv.wait(lck, [this]() { return !_hit || _hitStop; });
            if (_hitStop)
                return false;

            _hitParams = std::make_tuple(hook, GetCurrentThreadId(), isReturning, std::move(event));
            _hit = true;
            _hitCv.notify_all();

            return true;
        }

        void Victim::hookHLReturnStackPush(uintptr_t value)
        {
            Util::slimtls_guard tlsguard(_tls);
            auto stack = (return_stack_t*)_tlsReturnStack.get(false);
            if (!stack)
            {
                stack = new return_stack_t();
                _tlsReturnStack.set((uintptr_t)stack);
            }
            stack->push(value);
        }

        uintptr_t Victim::hookHLReturnStackPop()
        {
            Util::slimtls_guard tlsguard(_tls);
            auto stack = (return_stack_t*)_tlsReturnStack.get();
            auto value = stack->top();
            stack->pop();
            return value;
        }

        void DollDbg::Victim::Victim::hookHLThreadFini()
        {
            Util::slimtls_guard tlsguard(_tls);
            auto stack = (return_stack_t*)_tlsReturnStack.get(false);
            if (stack)
            {
                delete stack;
                _tlsReturnStack.set(0);
            }
        }
    }
}