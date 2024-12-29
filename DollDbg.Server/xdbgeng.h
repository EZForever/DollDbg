#pragma once
#include <dolldbg/dolldbg.h>
#include <dolldbg/util/com.h>

#include <functional>

#include <dbgeng.h>

// For DbgEng API nonsense
#ifdef UNICODE
#define DBGENGT(x) x##Wide
#else
#define DBGENGT(x) x
#endif

namespace DollDbg
{
    namespace Server
    {
        using IDebugClient = ::IDebugClient5; // Base object, target creation
        using IDebugControl = ::IDebugControl4; // Target control
        using IDebugDataSpaces = ::IDebugDataSpaces; // Memory
        using IDebugRegisters = ::IDebugRegisters; // Registers
        //using IDebugSymbols = ::IDebugSymbols; // Symbol evaluation
        using IDebugSystemObjects = ::IDebugSystemObjects; // Threads

        using input_callback_t = std::function<void(bool)>;
        using output_callback_t = std::function<void(ULONG, const string_t&)>;

#if 0 // XXX: This one will not work with Util::ComClass since __uuidof(DebugBaseEventCallbacks) is not defined
        class DebugEventCallbacks : public ::DBGENGT(DebugBaseEventCallbacks)
        {
        protected:
            DebugEventCallbacks();
            DebugEventCallbacks(DebugEventCallbacks&) = delete;

            ULONG _ref;

        public:
            static DebugEventCallbacks* Create();

            // --- Implements DebugBaseEventCallbacks->IDebugEventCallbacks->IUnknown

            STDMETHOD(QueryInterface)(REFIID riid, void** ppvObject);
            STDMETHOD_(ULONG, AddRef)();
            STDMETHOD_(ULONG, Release)();

            // --- Extends DebugBaseEventCallbacks

            STDMETHOD(GetInterestMask)(PULONG Mask);

            STDMETHOD(CreateProcess)(ULONG64 ImageFileHandle, ULONG64 Handle, ULONG64 BaseOffset, ULONG ModuleSize, PCTSTR ModuleName, PCTSTR ImageName, ULONG CheckSum, ULONG TimeDateStamp, ULONG64 InitialThreadHandle, ULONG64 ThreadDataOffset, ULONG64 StartOffset);
            STDMETHOD(ExitProcess)(ULONG ExitCode);
            STDMETHOD(SystemError)(ULONG Error, ULONG Level);
        };
#endif

        class DebugInputCallbacks : public Util::ComClass<DebugInputCallbacks, ::IDebugInputCallbacks>
        {
            friend class Util::ComClass<DebugInputCallbacks, ::IDebugInputCallbacks>;

        protected:
            DebugInputCallbacks(input_callback_t callback);

            input_callback_t _callback;

        public:
            // --- Implements IDebugInputCallbacks

            STDMETHOD(StartInput)(ULONG BufferSize);
            STDMETHOD(EndInput)();
        };

        class DebugOutputCallbacks : public Util::ComClass<DebugOutputCallbacks, ::DBGENGT(IDebugOutputCallbacks)>
        {
            friend class Util::ComClass<DebugOutputCallbacks, ::DBGENGT(IDebugOutputCallbacks)>;

        protected:
            DebugOutputCallbacks(output_callback_t callback);

            output_callback_t _callback;

        public:
            // --- Implements IDebugOutputCallbacks

            STDMETHOD(Output)(ULONG Mask, PCTSTR Text);
        };
    }
}