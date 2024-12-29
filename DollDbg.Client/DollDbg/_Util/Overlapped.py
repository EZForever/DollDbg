from win32api import CloseHandle
from win32event import CreateEvent

from .WinTypes import HANDLE, PyHANDLE, OVERLAPPED, PyOVERLAPPED

class overlapped_t:
    def __init__(self, handle: PyHANDLE = HANDLE()):
        self._overlapped = OVERLAPPED()
        if handle:
            self._ownHandle = False
            self._overlapped.hEvent = handle
        else:
            self._ownHandle = True
            self._overlapped.hEvent = CreateEvent(None, True, False, None)
    
    def get(self) -> PyOVERLAPPED:
        return self._overlapped

    def setOffset(self, offset: int) -> None:
        self._overlapped.Internal = 0
        self._overlapped.InternalHigh = 0
        self._overlapped.Offset = offset & 0xffffffff
        self._overlapped.OffsetHigh = (offset >> 32) & 0xffffffff

    def close(self) -> None:
        if self._ownHandle:
            CloseHandle(self._overlapped.hEvent)

