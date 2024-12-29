#include "xdbgeng.h"

#include <intrin.h>

namespace DollDbg
{
    namespace Server
    {
        DebugEventCallbacks* DebugEventCallbacks::Create()
        {
            DebugEventCallbacks* obj = new DebugEventCallbacks();
            obj->AddRef();
            return obj;
        }

        DebugEventCallbacks::DebugEventCallbacks()
            : _ref(0)
        {
            // Empty
        }

        STDMETHODIMP DebugEventCallbacks::QueryInterface(REFIID riid, void** ppvObject)
        {
            if (!ppvObject)
                return E_POINTER;

            if (riid != IID_IUnknown && riid != __uuidof(IDebugEventCallbacks))
            {
                *ppvObject = nullptr;
                return E_NOINTERFACE;
            }

            this->AddRef();
            *ppvObject = this;
            return S_OK;
        }

        STDMETHODIMP_(ULONG) DebugEventCallbacks::AddRef()
        {
            return _InterlockedIncrement(&this->_ref);
        }

        STDMETHODIMP_(ULONG) DebugEventCallbacks::Release()
        {
            LONG lref = _InterlockedDecrement(&this->_ref);
            if (lref == 0)
                delete this;
            return lref;
        }

        STDMETHODIMP DebugEventCallbacks::GetInterestMask(PULONG Mask)
        {
            *Mask = DEBUG_EVENT_CREATE_PROCESS
                | DEBUG_EVENT_EXIT_PROCESS
                | DEBUG_EVENT_SYSTEM_ERROR;
            return S_OK;
        }

        STDMETHODIMP DebugEventCallbacks::CreateProcess(ULONG64 ImageFileHandle, ULONG64 Handle, ULONG64 BaseOffset, ULONG ModuleSize, PCTSTR ModuleName, PCTSTR ImageName, ULONG CheckSum, ULONG TimeDateStamp, ULONG64 InitialThreadHandle, ULONG64 ThreadDataOffset, ULONG64 StartOffset)
        {
            return DEBUG_STATUS_NO_CHANGE;
        }

        STDMETHODIMP DebugEventCallbacks::ExitProcess(ULONG ExitCode)
        {
            return DEBUG_STATUS_NO_CHANGE;
        }

        STDMETHODIMP DebugEventCallbacks::SystemError(ULONG Error, ULONG Level)
        {
            return DEBUG_STATUS_NO_CHANGE;
        }
    }
}