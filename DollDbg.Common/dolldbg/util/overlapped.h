#pragma once
#include <dolldbg/dolldbg.h>
#include <dolldbg/util/syserr.h>

#include <stdexcept>

namespace DollDbg
{
    namespace Util
    {
        struct overlapped_t
        {
        protected:
            overlapped_t(overlapped_t&) = delete;

            OVERLAPPED _overlapped;
            bool _ownHandle;

        public:
            overlapped_t()
                : _ownHandle(true), _overlapped{ 0 }
            {
                HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
                if (hEvent == NULL)
                    throw Util::make_syserr("overlapped_t()::(): CreateEvent() failed");
                _overlapped.hEvent = hEvent;
            }

            overlapped_t(overlapped_t&& other) noexcept
                : _ownHandle(false), _overlapped{ 0 }
            {
                *this = std::move(other);
            }

            overlapped_t(HANDLE hEvent)
                : _ownHandle(false), _overlapped{ 0 }
            {
                if (hEvent == NULL)
                    throw std::invalid_argument("overlapped_t()::(): Invalid hEvent");
                _overlapped.hEvent = hEvent;
            }

            ~overlapped_t()
            {
                if (_overlapped.hEvent != NULL && _ownHandle)
                    CloseHandle(_overlapped.hEvent);

                _overlapped.hEvent = NULL;
            }

            overlapped_t& operator=(overlapped_t&& other) noexcept
            {
                std::swap(_overlapped, other._overlapped);
                std::swap(_ownHandle, other._ownHandle);
                return *this;
            }

            operator LPOVERLAPPED()
            {
                return &_overlapped;
            }

            void setOffset(uint64_t offset)
            {
                _overlapped.Internal = 0;
                _overlapped.InternalHigh = 0;
                _overlapped.Offset = (DWORD)offset;
                _overlapped.OffsetHigh = (DWORD)(offset >> 32);
            }
        };
    }
}