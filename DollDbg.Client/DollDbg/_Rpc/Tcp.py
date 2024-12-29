import asyncio
from asyncio.streams import StreamReader, StreamWriter

from .Interfaces import ITransportFactory
from ._AsyncIO import AsyncIOTransport

class TcpTransport(AsyncIOTransport):
    def __init__(self, reader: StreamReader, writer: StreamWriter):
        super().__init__(reader, writer)

class TcpClientTransportFactory(ITransportFactory[TcpTransport]):
    def __init__(self, host: str, port: int):
        self._host = host
        self._port = port

    # --- Implements ITransportFactory

    async def connect(self) -> TcpTransport:
        reader, writer = await asyncio.open_connection(self._host, self._port)
        return TcpTransport(reader, writer)
    
    async def close(self) -> None:
        pass

