import enum

from typing import Callable, Coroutine, Union, TYPE_CHECKING

if TYPE_CHECKING:
    from .VictimProxy import VictimProxy

@enum.unique
class OutputLevel(enum.Enum):
    '''
        Possible levels of Victim output.
    '''

    Normal = enum.auto()
    ''' Normal output (produced by Victim debugger or the debuggee). '''
    Warning = enum.auto()
    ''' Warnings. '''
    Error = enum.auto()
    ''' Errors. '''
    Prompt = enum.auto()
    ''' Prompt update notification. '''
    Supressed = enum.auto()
    ''' This output level will never be passed to handler. '''

output_t = Union[Callable[['VictimProxy', OutputLevel, str], None], Callable[['VictimProxy', OutputLevel, str], Coroutine[None, None, None]]]
'''
    Valid prototypes for output handlers:
    ```py
    def output_handler(victim: VictimProxy, level: OutputLevel, message: str) -> None:
        ...

    async def output_handler(victim: VictimProxy, level: OutputLevel, message: str) -> None:
        ...
    ```
'''

