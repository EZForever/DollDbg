from asyncio.streams import StreamReader, StreamWriter

from .Interfaces import ITransport

class AsyncIOTransport(ITransport):
    def __init__(self, reader: StreamReader, writer: StreamWriter):
        self._reader = reader
        self._writer = writer

    # --- Implements ITransport

    async def send(self, data: bytes) -> None:
        self._writer.write(data)
        await self._writer.drain()

    async def recv(self, size: int) -> bytes:
        return await self._reader.readexactly(size)

    async def close(self) -> None:
        self._writer.close()
        await self._writer.wait_closed()

