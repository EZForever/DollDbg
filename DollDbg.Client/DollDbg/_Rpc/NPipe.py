import asyncio

from win32con import NMPWAIT_USE_DEFAULT_WAIT, GENERIC_READ, GENERIC_WRITE, OPEN_EXISTING, FILE_FLAG_OVERLAPPED
from win32file import CreateFile
from win32pipe import WaitNamedPipe

from .._Util.WinTypes import HANDLE, PyHANDLE

from .Interfaces import ITransportFactory
from ._Overlapped import OverlappedTransport

class NPipeTransport(OverlappedTransport):
    def __init__(self, handle: PyHANDLE):
        super().__init__(handle)

class NPipeClientTransportFactory(ITransportFactory[NPipeTransport]):
    def __init__(self, name: str):
        self._name = name
    
    # --- Implements NPipeClientTransportFactory

    async def connect(self) -> NPipeTransport:
        await asyncio.to_thread(WaitNamedPipe, self._name, NMPWAIT_USE_DEFAULT_WAIT)

        pipe = CreateFile(self._name, GENERIC_READ | GENERIC_WRITE, 0, None, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, HANDLE())
        return NPipeTransport(pipe)
    
    async def close(self) -> None:
        pass

