from abc import ABC, abstractmethod
from typing import Generic, TypeVar

class ITransport(ABC):
    @abstractmethod
    async def send(self, data: bytes) -> None:
        raise NotImplementedError

    @abstractmethod
    async def recv(self, size: int) -> bytes:
        raise NotImplementedError

    @abstractmethod
    async def close(self) -> None:
        raise NotImplementedError

Transport = TypeVar('Transport', bound = ITransport)
class ITransportFactory(ABC, Generic[Transport]):
    @abstractmethod
    async def connect(self) -> Transport:
        raise NotImplementedError

    @abstractmethod
    async def close(self) -> None:
        raise NotImplementedError

