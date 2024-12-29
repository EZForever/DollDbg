import inspect
import itertools
import flatbuffers
from typing import Any, Callable

# usage:
# from Generated.DollDbg.Protocol.Dummy.Dummy import Dummy
# CreateDummy = CreateWrapper(Dummy)
class CreateWrapper:
    _METHOD_BLACKLIST = ['Init']
    _METHOD_SUFFIX_BLACKLIST = ['AsNumpy', 'Length', 'IsNone']

    def __init__(self, rootType: type):
        self._start: Callable[[flatbuffers.Builder], None] = None
        self._end: Callable[[flatbuffers.Builder], None] = None
        self._fields: dict[str, Callable[[flatbuffers.Builder, Any], None]] = None

        # Get all data fields
        members = inspect.getmembers(rootType, inspect.isfunction)
        fields = [k for k, _ in members \
            if k not in self._METHOD_BLACKLIST \
                and not any([k.endswith(x, 1) for x in self._METHOD_SUFFIX_BLACKLIST])]

        # Get Start, End and AddXXX functions
        module = inspect.getmodule(rootType)
        members = dict(inspect.getmembers(module, inspect.isfunction))

        self._start = members['Start']
        self._end = members['End']
        self._fields = {k: members['Add' + k] for k in fields}

    def __call__(self, builder: flatbuffers.Builder, *args: Any, **kwargs: Any) -> int:
        values = dict(itertools.zip_longest(self._fields.keys(), args)) | kwargs
        self._start(builder)
        for field, value in values.items():
            self._fields[field](builder, value)
        return self._end(builder)

def pack(ctor: Callable[[flatbuffers.Builder], int]) -> bytes:
    builder = flatbuffers.Builder()
    builder.Finish(ctor(builder))
    return bytes(builder.Output())

