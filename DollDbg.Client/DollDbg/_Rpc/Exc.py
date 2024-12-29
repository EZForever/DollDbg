import enum

from struct import Struct
from dataclasses import dataclass

_UInt32Struct = Struct('!I')

def _uint32_pack(data: int) -> str:
    return _UInt32Struct.pack(data).decode()

def _uint32_unpack(data: str) -> int:
    return _UInt32Struct.unpack(data.encode())[0]

@dataclass
class error_t:
    @enum.unique
    class category_t(enum.IntEnum):
        system = _uint32_unpack('ESYS')
        transport = _uint32_unpack('TRSP')
        application = _uint32_unpack('EAPP')

        def __str__(self) -> str:
            return _uint32_pack(self)

    @enum.unique
    class transport_code_t(enum.IntEnum):
        connection_closed = _uint32_unpack('DEAD')
        cancelled = _uint32_unpack('ABRT')
        unknown_channel = _uint32_unpack('ECHN')
        invalid_payload = _uint32_unpack('WHAT')

    # ---

    category: category_t
    code: int

    # ---

    _struct = Struct('!II')
    size = _struct.size

    @classmethod
    def unpack(cls, data: bytes) -> 'error_t':
        category, code = cls._struct.unpack(data)
        return cls(cls.category_t(category), code)

    def pack(self) -> bytes:
        return self._struct.pack(self.category.value, self.code)

class RpcException(RuntimeError):
    def __init__(self, what: str, category: error_t.category_t, code: int):
        super().__init__(what)
        self._error = error_t(category, code)

    def __str__(self) -> str:
        return f'{super().__str__()}, code = {self._error.category}:{hex(self._error.code)}'

    def error(self) -> error_t:
        return self._error

class RpcSystemException(RpcException):
    def __init__(self, what: str, code: int):
        super().__init__(what, error_t.category_t.system, code)

class RpcTransportException(RpcException):
    def __init__(self, what: str, code: int):
        super().__init__(what, error_t.category_t.transport, code)

class RpcApplicationException(RpcException):
    def __init__(self, what: str, code: int):
        super().__init__(what, error_t.category_t.application, code)

