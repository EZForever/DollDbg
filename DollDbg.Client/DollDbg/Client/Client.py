import enum
import asyncio
import warnings
import dataclasses

from typing import Coroutine, Optional, Union

import DollDbg._P as P
import DollDbg._Protocol as Protocol

from .Output import OutputLevel, output_t
from .Breakpoint import BpAction, breakpoint_t
from .VictimProxy import VictimProxy, STILL_ACTIVE, PROMPT_INPUT
from .._Rpc import Rpc, RpcException, error_t, TcpClientTransportFactory, NPipeClientTransportFactory

_DBGENG_FAIL = 0x80040205

_RPC_VOID = Protocol.Serialize(P.VoidT())
_RPC_ACTION_MAPPING = {
    BpAction.Go.value: P.Client.HookAction.Go,
    BpAction.Skip.value: P.Client.HookAction.Skip,
    BpAction.BreakOnReturn.value: P.Client.HookAction.BreakOnReturn
}
_RPC_OUTPUT_MAPPING = {
    P.Client.OutputType.Normal: OutputLevel.Normal,
    P.Client.OutputType.Error: OutputLevel.Error,
    P.Client.OutputType.Warning: OutputLevel.Warning,
    P.Client.OutputType.Prompt: OutputLevel.Prompt,
    P.Client.OutputType.Debuggee: OutputLevel.Normal,
    P.Client.OutputType.Status: OutputLevel.Supressed,
}

@dataclasses.dataclass
class _victim_t:
    proxy: VictimProxy
    output: Optional[output_t]

    ready: bool = dataclasses.field(default = False)
    readyEvt: asyncio.Event = dataclasses.field(default_factory = asyncio.Event)
    promptCv: asyncio.Condition = dataclasses.field(default_factory = asyncio.Condition)
    bps: dict[int, breakpoint_t] = dataclasses.field(default_factory = dict)
    pendingCommand: Optional[asyncio.Task] = dataclasses.field(default = None)

@enum.unique
class MachineType(enum.IntEnum):
    '''
        IMAGE_FILE_MACHINE_XXX values supported by DollDbg.

        Reference: https://docs.microsoft.com/en-us/windows/win32/sysinfo/image-file-machine-constants
    '''
    Unknown = 0
    I386 = 0x014c
    ARM = 0x01c0
    AMD64 = 0x8664
    ARM64 = 0xAA64

    @property
    def is64Bit(self) -> bool:
        '''
            Get if the given machine type is 64-bit.
        '''
        return self in (self.AMD64, self.ARM64)

class DbgEngException(RuntimeError):
    '''
        Error caused by the Debugger Engine.
    '''
    pass

class ClientWarning(RuntimeWarning):
    '''
        Warning category used by DollDbg Client.
    '''
    pass

# XXX: This should actually be exception but that will interfere with RPC
warnings.simplefilter('always', ClientWarning)

class Client:
    '''
        DollDbg Client object.
    '''

    def __init__(self, connStr: str):
        '''
            Create a new DollDbg Client instance.

            `connStr`: Connection string to the Server instance (supports `tcp:` and `npipe:`)
        '''
        self._connStr = connStr
        self._victimsCv = asyncio.Condition()
        self._closeEvt = asyncio.Event()

        self._rpc: Rpc = None
        self._serverInfo: tuple[MachineType, str] = None
        self._victims: dict[int, _victim_t] = dict()
        self._defaultOutput: Optional[output_t] = None

    async def __aenter__(self) -> 'Client':
        await self.ready()
        return self

    async def __aexit__(self, exc_type, exc_value, traceback) -> None:
        await self.close()

    def _assertClientReady(self) -> None:
        if self._rpc is None or self._closeEvt.is_set():
            raise RuntimeError('Client not ready')

    def _assertVictimReady(self, pid: int, pre_ready: bool = False) -> _victim_t:
        self._assertClientReady()
        if pid not in self._victims:
            raise ValueError('Victim not found')
        
        victim = self._victims[pid]
        if not victim.ready and not pre_ready:
            raise RuntimeError('Victim not ready')
        
        return victim

    if '--- RPC callbacks ---':

        async def _onRpcOffline(self, req: bytes) -> bytes:
            offlineReq = Protocol.Unserialize(P.Client.OfflineReqT, req)
            pid = offlineReq.pid
            code = offlineReq.code

            victim = self._assertVictimReady(pid)
            victim.proxy._exit_code = code
            async with self._victimsCv:
                del self._victims[pid]
                self._victimsCv.notify_all()

            return _RPC_VOID

        async def _onRpcNotifyHook(self, req: bytes) -> bytes:
            notifyHookReq = Protocol.Unserialize(P.Client.NotifyHookReqT, req)
            pid = notifyHookReq.pid
            tid = notifyHookReq.tid
            va = notifyHookReq.va
            is_returning = notifyHookReq.isReturning

            victim = self._assertVictimReady(pid, pre_ready = True)
            if va == 0:
                async with self._victimsCv:
                    victim.ready = True
                    self._victimsCv.notify_all()
                await victim.readyEvt.wait()
                action = BpAction.Go
            else:
                if va not in victim.bps:
                    warnings.warn('Unknown breakpoint hit - ignoring breakpoint', ClientWarning)
                    action = BpAction.Go
                else:
                    try:
                        action = victim.bps[va](victim.proxy, tid, va, is_returning)
                        if isinstance(action, Coroutine):
                            action = await action
                    except Exception as exc:
                        warnings.warn('Breakpoint handler exception - ignoring breakpoint', ClientWarning, source = exc)
                        action = BpAction.Go

            notifyHookResp = P.Client.NotifyHookRespT()
            notifyHookResp.action = _RPC_ACTION_MAPPING[action.action]
            if action.return_value is None:
                notifyHookResp.returnValue = None
            else:
                notifyHookResp.returnValue = P.UInt64T()
                notifyHookResp.returnValue.v = action.return_value
            if action.stack_bytes is None:
                notifyHookResp.stackBytes = None
            else:
                notifyHookResp.stackBytes = P.UInt64T()
                notifyHookResp.stackBytes.v = action.stack_bytes
            return Protocol.Serialize(notifyHookResp)

        async def _onRpcOutput(self, req: bytes) -> bytes:
            outputReq = Protocol.Unserialize(P.Client.OutputReqT, req)
            pid = outputReq.pid
            type_ = outputReq.type
            message = outputReq.message.decode()

            # FIXME: Currently output may happen before victim ready (initial hook), causing data loss
            #victim = self._assertVictimReady(pid, pre_ready = False)
            if pid in self._victims:
                victim = self._victims[pid]
                if victim.output is not None:
                    try:
                        level = _RPC_OUTPUT_MAPPING[type_]
                        result = victim.output(victim.proxy, level, message)
                        if isinstance(result, Coroutine):
                            await result
                    except Exception as exc:
                        warnings.warn('Output handler exception - ignoring', ClientWarning, source = exc)

                if type_ == P.Client.OutputType.Prompt:
                    victim.proxy._prompt = message
                    async with victim.promptCv:
                        victim.promptCv.notify_all()

            return _RPC_VOID

        async def _onRpcShutdown(self, req: bytes) -> bytes:
            self._closeEvt.set()
            return _RPC_VOID

        async def _onRpcException(self, exc: Exception) -> None:
            warnings.warn('RPC exception', ClientWarning, source = exc)
            self._closeEvt.set()

    if '--- Victim routines ---':
        
        async def _victimNew(self, pid: int, name: str, output: Optional[output_t]) -> VictimProxy:
            proxy = VictimProxy(self, pid, name)
            async with self._victimsCv:
                self._victims[pid] = _victim_t(proxy, output)
                await self._victimsCv.wait_for(lambda: self._victims[pid].ready)
            # Populate prompt
            await self._victimInput(pid, '')
            return proxy

        async def _victimReady(self, pid: int) -> int:
            victim = self._assertVictimReady(pid)
            victim.readyEvt.set()

        async def _victimLoadLibrary(self, pid: int, path: str) -> int:
            self._assertVictimReady(pid)

            loadLibReq = P.Server.C.LoadLibReqT()
            loadLibReq.pid = pid
            loadLibReq.path = path
            try:
                resp = await self._rpc.call(P.Server.C.RpcChannel.LoadLib, Protocol.Serialize(loadLibReq))
            except RpcException as exc:
                if exc.error().category == error_t.category_t.system:
                    code = exc.error().code
                    raise OSError(0, 'LoadLibrary() failed', path, code)
                raise

            virtualAddr = Protocol.Unserialize(P.VirtualAddrT, resp)
            return virtualAddr.va

        async def _victimInput(self, pid: int, line: str) -> None:
            victim = self._assertVictimReady(pid)

            inputReq = P.Server.C.InputReqT()
            inputReq.pid = pid
            inputReq.line = line
            task = asyncio.create_task(self._rpc.call(P.Server.C.RpcChannel.Input, Protocol.Serialize(inputReq)))

            async with victim.promptCv:
                while True:
                    await victim.promptCv.wait()
                    prompt = victim.proxy._prompt
                    if prompt.endswith('>'):
                        break

            try:
                if prompt == PROMPT_INPUT:
                    if victim.pendingCommand is None:
                        # This command required input, return immediately
                        victim.pendingCommand = task
                    else:
                        # This is a line of input, and the command is demanding more
                        await task
                else:
                    # No command is executing, no need to keep the task
                    await task

                    if victim.pendingCommand is not None:
                        # This is a line of input, and the command is fulfilled
                        await victim.pendingCommand
                        victim.pendingCommand = None
            except RpcException as exc:
                if exc.error().category == error_t.category_t.system \
                    and exc.error().code == _DBGENG_FAIL:
                    raise DbgEngException('DbgEng command failed')
                raise

        async def _victimEval(self, pid: int, expr: str) -> Union[int, list[int]]:
            self._assertVictimReady(pid)

            evalReq = P.Server.C.EvalReqT()
            evalReq.pid = pid
            evalReq.expr = expr
            try:
                resp = await self._rpc.call(P.Server.C.RpcChannel.Eval, Protocol.Serialize(evalReq))
            except RpcException as exc:
                if exc.error().category == error_t.category_t.system \
                    and exc.error().code == _DBGENG_FAIL:
                    raise DbgEngException('DbgEng evaluation failed')
                raise
            
            number = Protocol.Unserialize(P.NumberT, resp)
            # XXX: This is a lazy approach
            result = number.number.v
            if isinstance(result, int):
                return result
            else:
                return result.v

        async def _victimBpSet(self, pid: int, va: int, handler: breakpoint_t) -> None:
            victim = self._assertVictimReady(pid)
            victim.bps[va] = handler

            bpSetReq = P.Server.C.BpSetReqT()
            bpSetReq.pid = pid
            bpSetReq.va = va
            await self._rpc.call(P.Server.C.RpcChannel.BpSet, Protocol.Serialize(bpSetReq))

        async def _victimBpClear(self, pid: int, va: int) -> None:
            victim = self._assertVictimReady(pid)

            bpSetReq = P.Server.C.BpSetReqT()
            bpSetReq.pid = pid
            bpSetReq.va = va
            await self._rpc.call(P.Server.C.RpcChannel.BpSet, Protocol.Serialize(bpSetReq))

            del victim.bps[va]

        async def _victimWait(self, pid: int) -> None:
            async with self._victimsCv:
                await self._victimsCv.wait_for(lambda: pid not in self._victims)

        async def _victimDetach(self, pid: int) -> None:
            self._assertClientReady()

            if pid in self._victims:
                procId = P.ProcIdT()
                procId.pid = pid
                await self._rpc.call(P.Server.C.RpcChannel.Detach, Protocol.Serialize(procId))

            await self._victimWait(pid)

    @property
    def serverInfo(self) -> tuple[MachineType, str]:
        '''
            Get Server identity as a (machine_type, name) tuple.
        '''
        if self._serverInfo is None:
            raise RuntimeError('Not connected to the Server')
        return self._serverInfo

    @property
    def defaultOutput(self) -> Optional[output_t]:
        '''
            The default output handler to use.
        '''
        return self._defaultOutput

    @defaultOutput.setter
    def defaultOutput(self, value: Optional[output_t]) -> None:
        self._defaultOutput = value

    def output(self, handler: output_t) -> output_t:
        '''
            Syntactic sugar to define and set the default output handler at the same time.

            ```py
            @client.output
            async def onOutput(*args):
                ...
            ```
        '''
        self._defaultOutput = handler
        return handler

    async def ready(self) -> None:
        '''
            Initialize Client and connect to the Server. Call this before any other call.
        '''
        rpc = Rpc()
        rpc.registerChannel(P.Client.RpcChannel.Offline, self._onRpcOffline)
        rpc.registerChannel(P.Client.RpcChannel.NotifyHook, self._onRpcNotifyHook)
        rpc.registerChannel(P.Client.RpcChannel.Output, self._onRpcOutput)
        rpc.registerChannel(P.Client.RpcChannel.Shutdown, self._onRpcShutdown)
        rpc.onException()

        parts = self._connStr.strip().split(':', 1)
        if len(parts) < 2:
            raise ValueError(f'Missing connection protocol: {repr(self._connStr)}')
        
        proto, rest = parts
        if proto == 'tcp':
            # tcp:<host>:<port>
            portBegin = rest.rfind(':', max(0, rest.rfind(']')))
            if portBegin <= 0:
                raise ValueError(f'Invalid connection address: {repr(self._connStr)}')
            
            host = rest[ : portBegin]
            port = int(rest[portBegin + 1 : ])
            factory = TcpClientTransportFactory(host, port)
        elif proto == 'npipe':
            # npipe:\\.\pipe\<pipe_name>
            factory = NPipeClientTransportFactory(rest)
        else:
            raise ValueError(f'Invalid connection protocol: {repr(self._connStr)}')
        
        await rpc.ready(factory)
        self._rpc = rpc

        resp = await rpc.call(P.Server.C.RpcChannel.Online, _RPC_VOID)
        onlineResp = Protocol.Unserialize(P.Server.C.OnlineRespT, resp)
        machineType = onlineResp.machineType
        name = onlineResp.name.decode()
        if machineType not in MachineType.__members__.values() or machineType == MachineType.Unknown:
            raise RuntimeError(f'Unknown Server machine type: 0x{machineType:04x}')
        self._serverInfo = (MachineType(machineType), name)

    async def procs(self, prefix: Optional[str] = None) -> tuple[int, str]:
        '''
            Async generator of (pid, name) tuples representing processes running on the Server.

            `prefix`: Optionally filter process name prefixes (case insensitive)

            The list is also filtered server-side to only contain attach-friendly processes.
        '''
        self._assertClientReady()
        
        procsReq = P.Server.C.ProcsReqT()
        procsReq.prefix = prefix
        resp = await self._rpc.call(P.Server.C.RpcChannel.Procs, Protocol.Serialize(procsReq))
        procsResp = Protocol.Unserialize(P.Server.C.ProcsRespT, resp)
        for procResp in procsResp.procs:
            yield (procResp.pid, procResp.name.decode())

    async def kill(self, proc: Union[int, str]) -> None:
        '''
            Kill process(es) on the Server with given pid or name.
        '''
        self._assertClientReady()
        
        killReq = P.Server.C.KillReqT()
        if isinstance(proc, int):
            killReq.pid = proc
        else:
            killReq.name = proc
        await self._rpc.call(P.Server.C.RpcChannel.Kill, Protocol.Serialize(killReq))

    async def launch(self, cmd: Optional[str], arg: Optional[str] = None, dir: Optional[str] = None, output: Optional[output_t] = ...) -> VictimProxy:
        '''
            Launch a new process as Victim.
        '''
        if cmd is None and arg is None:
            raise ValueError('cmd and arg cannot be both None')

        self._assertClientReady()

        launchReq = P.Server.C.LaunchReqT()
        launchReq.cmd = cmd
        launchReq.arg = arg
        launchReq.dir = dir
        resp = await self._rpc.call(P.Server.C.RpcChannel.Launch, Protocol.Serialize(launchReq))
        proc = Protocol.Unserialize(P.ProcT, resp)
        return await self._victimNew(proc.pid, proc.name.decode(), self._defaultOutput if output is ... else output)

    async def attach(self, pid: int, output: Optional[output_t] = ...) -> VictimProxy:
        '''
            Attach to a process as Victim.
        '''
        self._assertClientReady()

        procId = P.ProcIdT()
        procId.pid = pid
        resp = await self._rpc.call(P.Server.C.RpcChannel.Attach, Protocol.Serialize(procId))
        proc = Protocol.Unserialize(P.ProcT, resp)
        return await self._victimNew(proc.pid, proc.name.decode(), self._defaultOutput if output is ... else output)

    async def close(self) -> None:
        '''
            Detach from all Victims and disconnect from the Server.
        '''
        if self._rpc is not None:
            await self._rpc.call(P.Server.C.RpcChannel.Offline, _RPC_VOID)
            await self._closeEvt.wait()
            await self._rpc.close()
            self._rpc = None

            for victim in self._victims.values():
                victim.proxy._exit_code = STILL_ACTIVE
            async with self._victimsCv:
                self._victims.clear()
                self._victimsCv.notify_all()

