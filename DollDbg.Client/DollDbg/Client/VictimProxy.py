import asyncio

from typing import Callable, Coroutine, Optional, Union, TYPE_CHECKING
from functools import partial

if TYPE_CHECKING:
    from .Client import Client
    from .Breakpoint import breakpoint_t

STILL_ACTIVE = 259
'''
    "Exit code" of a detached Victim, which still have an active process.

    Reference: https://docs.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getexitcodeprocess#remarks
'''

PROMPT_BUSY = '*BUSY*'
'''
    Value of the prompt when input is not possible.
'''

PROMPT_INPUT = 'Input>'
'''
    Value of the prompt when non-command input is required.
'''

class VictimProxy:
    '''
        Proxy of available operations on a running Victim.
    '''

    def __init__(self, client: 'Client', pid: int, name: str):
        '''
            Create a Victim proxy instance.

            User should not instantiate this class directly; use `Client.launch()` or `Client.attach()` instead.
        '''
        self._client = client
        self._pid = pid
        self._name = name

        self._prompt: str = None
        self._exit_code: Optional[int] = None
        self._deferred_bps: set[Coroutine[None, None, None]] = set()

    async def __aenter__(self) -> 'VictimProxy':
        # NOTE: self.ready() needs to be called by user
        return self

    async def __aexit__(self, exc_type, exc_value, traceback) -> None:
        pass # TODO

    async def _assertAddress(self, va: Union[int, str]) -> int:
        if not isinstance(va, int):
            va_eval = await self.eval(va)
            if not isinstance(va_eval, int):
                raise ValueError(f'Invalid va {repr(va)}, evaluted to be {repr(va_eval)}')
            va = va_eval
        if va == 0:
            raise ValueError(f'va evaluated to be NULL')
        return va

    def _bpWrapper(self, va: Union[int, str], handler: 'breakpoint_t') -> 'breakpoint_t':
        self._deferred_bps.add(asyncio.create_task(self.bpSet(va, handler)))
        return handler

    @property
    def pid(self) -> int:
        '''
            Get Victim pid.
        '''
        return self._pid

    @property
    def name(self) -> str:
        '''
            Get Victim name.
        '''
        return self._name

    @property
    def prompt(self) -> str:
        if self._prompt is None:
            raise RuntimeError('Victim prompt not populated')
        return self._prompt

    @property
    def exit_code(self) -> Optional[int]:
        '''
            Get Victim exit code, or None if it's still available.

            For detached Victims, the exit code will be `STILL_ACTIVE`.

            User should not call any method in this proxy except `close()` if `exit_code()` returns any value other than `None`.
        '''
        return self._exit_code

    async def ready(self) -> None:
        '''
            Set all breakpoints defined by `bp()` and continue execution from the initial break.

            User must call this iff after setting necessary breakpoints.
        '''
        if len(self._deferred_bps) > 0:
            await asyncio.wait(self._deferred_bps)
            self._deferred_bps.clear()
        await self._client._victimReady(self._pid)

    async def loadSymbol(self, module: Optional[str] = None) -> None:
        '''
            Ensure symbols are loaded for the given module, or load symbols for all modules loaded in Victim.

            Symbol paths must be correctly configured on the Server for this to take effect.

            Based on `input('ld ...')` or `input('.reload /f')` but with prompt checks.
        '''
        if self._prompt in (PROMPT_BUSY, PROMPT_INPUT):
            raise RuntimeError('Executing command is not possible at this moment')
        await self.input('.reload /f' if module is None else f'ld {module}')

    async def loadLibrary(self, path: str) -> int:
        '''
            Issue a `LoadLibrary()` call on the Victim.
        '''
        return await self._client._victimLoadLibrary(self._pid, path)
    
    async def input(self, line: str) -> None:
        '''
            Send a line of input to the Debugger Engine.
        '''
        await self._client._victimInput(self._pid, line)

    async def eval(self, expr: str) -> Union[int, list[int]]:
        '''
            Evaluate the given expression in the Debugger Engine.
        '''
        return await self._client._victimEval(self._pid, expr)

    async def bpSet(self, va: Union[int, str], handler: 'breakpoint_t') -> None:
        '''
            Set a breakpoint and its handler.

            If `va` is not an integer, it is evalulated (with `eval()`) and treated as the address.
        '''
        if handler is None:
            raise ValueError('Breakpoint handler cannot be None (use bpClear() to remove breakpoint)')
        va = await self._assertAddress(va)
        await self._client._victimBpSet(self._pid, va, handler)

    async def bpClear(self, va: Union[int, str]) -> None:
        '''
            Clear a breakpoint.

            If `va` is not an integer, it is evalulated (with `eval()`) and treated as the address.
        '''
        va = await self._assertAddress(va)
        await self._client._victimBpClear(self._pid, va)

    def bp(self, va: Union[int, str]) -> Callable[['breakpoint_t'], 'breakpoint_t']:
        '''
            Syntactic sugar to set breakpoint and define handler at the same time.

            Note that this will only work before a `ready()` call.

            ```py
            @victim.bp('CreateProcessW')
            async def onCreateProcessW(*args):
                ...
            ```
        '''
        return partial(self._bpWrapper, va)

    async def wait(self) -> int:
        '''
            Wait for the Victim to exit, return the exit code.
        '''
        await self._client._victimWait(self._pid)
        return self._exit_code

    async def close(self) -> None:
        '''
            Detach from this Victim.
        '''
        await self._client._victimDetach(self._pid)

