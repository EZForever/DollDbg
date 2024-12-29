import asyncio
from struct import Struct
from typing import Coroutine, Callable, Optional, Union
from dataclasses import dataclass

from .Exc import error_t, RpcException, RpcTransportException, RpcSystemException, RpcApplicationException
from .Interfaces import ITransport, Transport, ITransportFactory

_CHANNEL_RESPONSE = 0
_LENGTH_ERROR = 0xffffffff

@dataclass
class _header_t:
    channel: int
    response: int
    length: int

    # ---

    _struct = Struct('!HHI')
    size = _struct.size

    @classmethod
    def unpack(cls, data: bytes) -> '_header_t':
        channel, response, length = cls._struct.unpack(data)
        return cls(channel, response, length)

    def pack(self) -> bytes:
        return self._struct.pack(self.channel, self.response, self.length)

handler_t = Union[Callable[[bytes], bytes], Callable[[bytes], Coroutine[None, None, bytes]]]
exc_callback_t = Union[Callable[[Exception], None], Callable[[Exception], Coroutine[None, None, None]]]

class Rpc:
    def __init__(self):
        self._transport: ITransport = None
        self._exc_callback: exc_callback_t = None
        self._receiver: asyncio.Task = None
        self._exc: Exception = None

        self._lastResponse = 0
        self._channels: dict[int, handler_t] = dict()
        self._requests: dict[int, asyncio.Future] = dict()
        self._workers: dict[int, asyncio.Task] = dict()

    async def _sendBuffer(self, channel: int, response: int, buffer: bytes) -> None:
        try:
            await self._transport.send(_header_t(channel, response, len(buffer)).pack())
            await self._transport.send(buffer)
        except asyncio.exceptions.CancelledError:
            raise
        except Exception as exc:
            self._exc = exc
            await self.close()

    async def _sendException(self, channel: int, response: int, exc: RpcException) -> None:
        try:
            await self._transport.send(_header_t(channel, response, _LENGTH_ERROR).pack())
            await self._transport.send(exc.error().pack())
        except asyncio.exceptions.CancelledError:
            raise
        except Exception as exc:
            self._exc = exc
            await self.close()

    async def _work(self, handler: handler_t, request: bytes, response: int) -> None:
        try:
            resp = handler(request)
            if isinstance(resp, Coroutine):
                resp = await resp
            await self._sendBuffer(_CHANNEL_RESPONSE, response, resp)
        except RpcException as exc:
            await self._sendException(_CHANNEL_RESPONSE, response, exc)
        except OSError as exc:
            await self._sendException(_CHANNEL_RESPONSE, response, RpcSystemException('Converted system exception', exc.winerror))
        except Exception as exc:
            await self._sendException(_CHANNEL_RESPONSE, response, RpcApplicationException('Converted unknown exception', 0x45504f4e))
        finally:
            del self._workers[response]

    async def _receive(self) -> None:
        try:
            while True:
                header = _header_t.unpack(await self._transport.recv(_header_t.size))
                buffer = await self._transport.recv(error_t.size if header.length == _LENGTH_ERROR else header.length)
                
                if header.channel == _CHANNEL_RESPONSE:
                    future = self._requests.pop(header.response)
                    if header.length == _LENGTH_ERROR:
                        error = error_t.unpack(buffer)
                        future.set_exception(RpcException('Incoming exception', error.category, error.code))
                    else:
                        future.set_result(buffer)
                else:
                    if header.channel not in self._channels:
                        await self._sendException(_CHANNEL_RESPONSE, header.response, RpcTransportException('Unknown channel', error_t.transport_code_t.unknown_channel.value))
                    else:
                        self._workers[header.response] = asyncio.create_task(self._work(self._channels[header.channel], buffer, header.response))
        except asyncio.exceptions.CancelledError:
            raise
        except Exception as exc:
            self._exc = exc
            await self.close()

    async def ready(self, factory: ITransportFactory[Transport]) -> None:
        self._transport = await factory.connect()
        self._receiver = asyncio.create_task(self._receive())

    async def call(self, channel: int, req: bytes) -> bytes:
        if self._transport is None:
            raise RuntimeError('Node not ready')

        if channel == _CHANNEL_RESPONSE:
            raise ValueError('Channel reserved for responses')

        self._lastResponse = (self._lastResponse + 1) & 0xffff
        response = self._lastResponse

        future = asyncio.Future()
        self._requests[response] = future
        await self._sendBuffer(channel, response, req)
        return await future

    def registerChannel(self, channel: int, handler: handler_t) -> None:
        if channel == _CHANNEL_RESPONSE:
            raise ValueError('Channel reserved for responses')

        self._channels[channel] = handler

    def unregisterChannel(self, channel: int) -> None:
        if channel == _CHANNEL_RESPONSE:
            raise ValueError('Channel reserved for responses')

        if channel in self._channels:
            del self._channels[channel]

    def onException(self, exc_callback: Optional[exc_callback_t] = None) -> None:
        self._exc_callback = exc_callback

    async def close(self) -> None:
        exc = RpcTransportException('Connection closed', error_t.transport_code_t.connection_closed.value)
        for future in list(self._requests.values()):
            future.set_exception(exc)
        
        for response, worker in self._workers.items():
            worker.cancel()
            await self._sendException(_CHANNEL_RESPONSE, response, exc)

        if self._receiver is not None and not self._receiver.done():
            self._receiver.cancel()
        if self._transport is not None:
            await self._transport.close()
        
        if self._exc is not None and self._exc_callback is not None:
            result = self._exc_callback(self._exc)
            if isinstance(result, Coroutine):
                await result

