import os
import flatbuffers

from typing import Protocol, Type, TypeVar

import DollDbg

class FlatBuffersNative(Protocol):
    @classmethod
    def InitFromBuf(cls, buf: bytes, pos: int):
        ...
    
    def Pack(self, builder: flatbuffers.Builder):
        ...

def Serialize(obj: FlatBuffersNative) -> bytes:
    builder = flatbuffers.Builder()
    builder.Finish(obj.Pack(builder))
    return bytes(builder.Output())

NativeType = TypeVar('NativeType', bound = FlatBuffersNative)
def Unserialize(nativeType: Type[NativeType], buffer: bytes) -> NativeType:
    n = flatbuffers.encode.Get(flatbuffers.packer.uoffset, buffer, 0)
    return nativeType.InitFromBuf(buffer, n)

