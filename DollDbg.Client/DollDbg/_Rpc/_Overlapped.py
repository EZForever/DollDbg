import asyncio

from win32api import CloseHandle
from win32file import ReadFile, WriteFile, GetOverlappedResult

from .._Util.WinTypes import PyHANDLE, AllocateReadBuffer
from .._Util.Overlapped import overlapped_t

from .Interfaces import ITransport

class OverlappedTransport(ITransport):
    def __init__(self, handle: PyHANDLE):
        self._handle = handle
        self._overlappedRead = overlapped_t()
        self._overlappedWrite = overlapped_t()
    
    # --- Implements ITransport

    async def send(self, data: bytes) -> None:
        size = len(data)
        offset = 0
        while offset < size:
            self._overlappedWrite.setOffset(0)
            WriteFile(self._handle, data[offset : ], self._overlappedWrite.get())
            
            transferred = await asyncio.to_thread(GetOverlappedResult, self._handle, self._overlappedWrite.get(), True)
            offset += transferred

    async def recv(self, size: int) -> bytes:
        data = bytearray()
        buf = AllocateReadBuffer(size)
        offset = 0
        while offset < size:
            self._overlappedRead.setOffset(0)
            ReadFile(self._handle, buf, self._overlappedRead.get())

            transferred = await asyncio.to_thread(GetOverlappedResult, self._handle, self._overlappedRead.get(), True)
            data.extend(buf[ : transferred])
            offset += transferred
        return bytes(data)

    async def close(self) -> None:
        # XXX: Currently pywin32 does not support CancelIoEx()
        #CancelIoEx(self._handle, self._overlappedRead)
        #CancelIoEx(self._handle, self._overlappedWrite)
        CloseHandle(self._handle)

