import enum
import dataclasses

from typing import Callable, Coroutine, Optional, Union, TYPE_CHECKING

if TYPE_CHECKING:
    from .VictimProxy import VictimProxy

@dataclasses.dataclass
class BpActionParams:
    '''
        Breakpoint actions with parameters.

        User should not instantiate this class directly; use `BpAction.XXX()` instead.
    '''
    action: int
    return_value: Optional[int] = None
    stack_bytes: Optional[int] = None

@enum.unique
class BpAction(BpActionParams, enum.Enum):
    '''
        Available actions when a breakpoint has been hit.
    '''

    Go = enum.auto()
    ''' Continue execution as if no breakpoint is set. '''
    Skip = enum.auto()
    ''' Skip execution of the function and return immediately. '''
    BreakOnReturn = enum.auto()
    ''' Continue exection and break on return with `isReturning = True`. '''

    def __call__(self, *, return_value: Optional[int] = None, stack_bytes: Optional[int] = None) -> BpActionParams:
        '''
            Add parameters to the action.
        '''
        return BpActionParams(self.value, return_value, stack_bytes)

breakpoint_t = Union[Callable[['VictimProxy', int, int, bool], BpActionParams], Callable[['VictimProxy', int, int, bool], Coroutine[None, None, BpActionParams]]]
'''
    Valid prototypes for breakpoint handlers:
    ```py
    def breakpoint_handler(victim: VictimProxy, tid: int, va: int, is_returning: bool) -> BpActionParams:
        ...

    async def breakpoint_handler(victim: VictimProxy, tid: int, va: int, is_returning: bool) -> BpActionParams:
        ...
    ```
'''

