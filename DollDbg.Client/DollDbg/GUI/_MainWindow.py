from pydoc import cli
import shlex
import string
import asyncio
import inspect
import platform
import tkinter as tk
import tkinter.ttk as ttk

from typing import Optional, Union
from asyncio.queues import Queue

import DollDbg

from ._MainWindowPage import MainWindowPage
from ._ExceptionDialog import ExceptionDialog
from ..Client import Client, DbgEngException, VictimProxy, OutputLevel, BpAction, BpActionParams, PROMPT_INPUT

_TABIDS = string.digits + string.ascii_uppercase

_OUTPUT_BANNER = inspect.cleandoc(f'''
    DollDbg GUI v{DollDbg.__version__}
    {DollDbg.__copyright__}

    Type ":help" or ":version" for more information.
''')

_uname = platform.uname()
_OUTPUT_VERSION = inspect.cleandoc(f'''
    {_uname.system} {_uname.release} Version {_uname.version} {_uname.machine}
    {platform.python_implementation()} Version {platform.python_version()}

    DollDbg Version {DollDbg.__version__} at {DollDbg.__path__[0]}
    {DollDbg.__copyright__}
''')

_OUTPUT_HELP = inspect.cleandoc('''
    All DollDbg commands are case sensitive and start with ":".

        :                           - Comment
        :version                    - Display DollDbg version
        :?, :help                   - Command help
        :q, :quit                   - Quit
        :cls, :clear                - Clear output
        :svr, :server <connStr>     - Connect to DollDbg Server
    
    Following commands need a Server connection:

        :ps, :procs [prefix]        - Show processes running on the Server
        :kill <pid>                 - Kill a process with given PID
        :killall <name>             - Kill all processes with given image name
        :launch <cmd> [dir]         - Launch a new process as Victim
        :attach <pid>               - Attach to process as Victim

    Following commands need to run on a Victim:

        :detach                     - Detach from process
        :inject <path>              - Call LoadLibrary()
        :bp <expr>                  - Set breakpoint
        :bc <expr>                  - Clear breakpoint
        :g, :go                     - Continue execution from a breakpoint
        :gu, :return                - Continue execution and break on return
        :gs, :skip [retval] [stack] - Skip execution
    
    For DbgEng command help, run "?" and/or ".help" on a Victim.
''')

class MainWindow(tk.Toplevel):
    def __init__(self, master: tk.Tk, **kwargs):
        super().__init__(master, **kwargs)
        self.title(f'DollDbg GUI')

        self._pages_ctl = ttk.Notebook(self)
        self._pages_ctl.pack(expand = 1, fill = tk.BOTH)
        self._pages_ctl.enable_traversal()
        
        self._tab_id = 0
        self._client: Client = None
        self._page0: MainWindowPage = None
        self._pages: dict[int, MainWindowPage] = dict()
        self._messages: Queue[tuple[Optional[MainWindowPage], str]] = Queue()

        self._createPage(None)
        self._page0.append(OutputLevel.Normal, _OUTPUT_BANNER + '\n\n')

    def _on_page_input(self, event: tk.Event) -> str:
        page: MainWindowPage = event.widget
        self._messages.put_nowait((page, page.event_data_input))
        return ''

    def _on_client_output(self, victim: VictimProxy, level: OutputLevel, message: str) -> None:
        # FIXME: Another layer of data loss... See Client/Client.py!Client_onRpcOutput()
        if victim.pid not in self._pages:
            return
        
        page = self._pages[victim.pid]
        if level == OutputLevel.Prompt:
            page.prompt = message
        else:
            page.append(level, message)

    async def _on_client_breakpoint(self, victim: VictimProxy, tid: int, va: int, is_returning: bool) -> BpActionParams:
        page = self._pages[victim.pid]
        page.append(OutputLevel.Warning, f'\nBreakpoint hit: Thread {hex(tid)}, VA {hex(va)}, {"returning" if is_returning else "calling"}\n\n')
        return await page.wait_bp_action()

    def _createPage(self, victim: Optional[VictimProxy]) -> None:
        page = MainWindowPage(self._pages_ctl, victim)
        page.bind('<<Input>>', self._on_page_input)

        if victim is None:
            self._page0 = page
            text = 'Server'
        else:
            self._pages[victim.pid] = page
            text = f'{victim.name} #{victim.pid}'
        
        if self._tab_id < len(_TABIDS):
            self._pages_ctl.add(page, text = f'{self._tab_id} {text}', underline = 0)
        else:
            self._pages_ctl.add(page, text = text)
        self._tab_id += 1

        self._pages_ctl.select(page)
        page.focus_input()

    async def _commandCommon(self, page: MainWindowPage, args: list[str]) -> bool:
        if args[0] == ':':
            # A single : denotes a line of comment
            return True
        elif args[0] in (':version'):
            # Display version
            page.append(OutputLevel.Normal, _OUTPUT_VERSION + '\n\n')
            return True
        elif args[0] in (':?', ':help'):
            # Command help
            page.append(OutputLevel.Normal, _OUTPUT_HELP + '\n\n')
            return True
        elif args[0] in (':q', ':quit'):
            # Close window
            self.destroy()
            return True
        elif args[0] in (':cls', ':clear'):
            # Clear screen
            page.clear()
            return True
        elif args[0] in (':svr', ':server'):
            # Connect to server
            if self._client is not None:
                page.append(OutputLevel.Warning, 'Already connected to a Server; ignoring.\n')
            elif len(args) != 2:
                page.append(OutputLevel.Error, 'Invalid argument count; try ":?".\n')
            else:
                try:
                    client = None
                    try:
                        client = Client(args[1])
                        client.defaultOutput = self._on_client_output
                        await client.ready()
                    except:
                        if client is not None:
                            await client.close()
                        raise
                    self._client = client
                except ValueError as exc:
                    page.append(OutputLevel.Error, str(exc) + '\n')
                else:
                    machine, name = self._client.serverInfo
                    page.append(OutputLevel.Normal, f'Connected to {name} ({machine.name}).\n')
            return True
        return self._client is not None and await self._commandServer(page, args)

    async def _commandServer(self, page: MainWindowPage, args: list[str]) -> bool:
        if args[0] in (':ps', ':procs'):
            # `ps` alternative
            if len(args) > 2:
                page.append(OutputLevel.Error, 'Invalid argument count; try ":?".\n')
            else:
                async for pid, name in self._client.procs(args[1] if len(args) > 1 else None):
                    page.append(OutputLevel.Normal, f'{pid}\t{name}\n')
            return True
        elif args[0] in (':kill'):
            # `kill` alternative
            if len(args) != 2:
                page.append(OutputLevel.Error, 'Invalid argument count; try ":?".\n')
            else:
                try:
                    pid = int(args[1])
                except ValueError:
                    page.append(OutputLevel.Error, 'Invalid PID.\n')
                else:
                    await self._client.kill(pid)
            return True
        elif args[0] in (':killall'):
            # `killall` alternative
            if len(args) != 2:
                page.append(OutputLevel.Error, 'Invalid argument count; try ":?".\n')
            else:
                await self._client.kill(args[1])
            return True
        elif args[0] in (':launch'):
            # Launch new Victim
            if len(args) not in (2, 3):
                page.append(OutputLevel.Error, 'Invalid argument count; try ":?".\n')
            else:
                arg = args[1]
                dir = args[2] if len(args) > 2 else None
                victim = await self._client.launch(None, arg, dir)
                self._createPage(victim)
                await victim.input('version')
            return True
        elif args[0] in (':attach'):
            # Attach & create Victim
            if len(args) != 2:
                page.append(OutputLevel.Error, 'Invalid argument count; try ":?".\n')
            else:
                try:
                    pid = int(args[1])
                except ValueError:
                    page.append(OutputLevel.Error, 'Invalid PID.\n')
                else:
                    victim = await self._client.attach(pid)
                    self._createPage(victim)
                    await victim.input('version')
            return True
        return False

    async def _commandVictim(self, page: MainWindowPage, args: list[str]) -> bool:
        if args[0] in (':detach'):
            # Detach
            if len(args) != 1:
                page.append(OutputLevel.Error, 'Invalid argument count; try ":?".\n')
            else:
                await page.victim.close()
            return True
        elif args[0] in (':inject'):
            # Call LoadLibrary()
            if len(args) != 2:
                page.append(OutputLevel.Error, 'Invalid argument count; try ":?".\n')
            else:
                try:
                    hmod = await page.victim.loadLibrary(args[1])
                except OSError as exc:
                    page.append(OutputLevel.Error, f'LoadLibrary("{args[1]}") failed, GetLastError() = {exc.winerror}\n')
                else:
                    page.append(OutputLevel.Normal, f'LoadLibrary("{args[1]}") = {hex(hmod)}\n')
            return True
        elif args[0] in (':bp'):
            # Set breakpoint
            if len(args) != 2:
                page.append(OutputLevel.Error, 'Invalid argument count; try ":?".\n')
            else:
                try:
                    await page.victim.bpSet(args[1], self._on_client_breakpoint)
                except DbgEngException as exc:
                    page.append(OutputLevel.Error, str(exc) + '\n')
            return True
        elif args[0] in (':bc'):
            # Clear breakpoint
            if len(args) != 2:
                page.append(OutputLevel.Error, 'Invalid argument count; try ":?".\n')
            else:
                try:
                    await page.victim.bpClear(args[1])
                except DbgEngException as exc:
                    page.append(OutputLevel.Error, str(exc) + '\n')
            return True
        elif args[0] in (':g', ':go'):
            # Breakpoint: Go
            if len(args) != 1:
                page.append(OutputLevel.Error, 'Invalid argument count; try ":?".\n')
            else:
                await page.notify_bp_action(BpAction.Go)
            return True
        elif args[0] in (':gu', ':return'):
            # Breakpoint: BreakOnReturn
            if len(args) != 1:
                page.append(OutputLevel.Error, 'Invalid argument count; try ":?".\n')
            else:
                await page.notify_bp_action(BpAction.BreakOnReturn)
            return True
        elif args[0] in (':gs', ':skip'):
            # Breakpoint: Skip
            if len(args) > 3:
                page.append(OutputLevel.Error, 'Invalid argument count; try ":?".\n')
            else:
                try:
                    retval = None if len(args) < 2 else await page.victim.eval(args[1])
                    stack = None if len(args) < 3 else await page.victim.eval(args[2])
                except DbgEngException as exc:
                    page.append(OutputLevel.Error, str(exc) + '\n')
                else:
                    await page.notify_bp_action(BpAction.Skip(return_value = retval, stack_bytes = stack))
            return True
        return await self._commandCommon(page, args)

    async def amsgloop(self, connStr: Optional[str]) -> None:
        try:
            if connStr is not None:
                try:
                    await self._commandCommon(self._page0, [':svr', connStr])
                except Exception as exc:
                    self._page0.append(OutputLevel.Error, 'Exception occurred on initial connection.\n')
                    ExceptionDialog(self, exc)

            while True:
                sender, message = await self._messages.get()
                try:
                    if sender is None:
                        # Internal message
                        if message == 'destroy':
                            # Window destorying
                            if self._client is not None:
                                await self._client.close()
                            break
                    elif message.lstrip().startswith(':') and (sender.victim is None or sender.prompt != PROMPT_INPUT):
                        # Command from server/victim page

                        # shlex.split() took care of stripping whitespaces
                        # NOTE: Comments must be off since `#` is a valid command in DbgEng
                        args = shlex.split(message, comments = False, posix = True)
                        
                        try:
                            consumed = await (self._commandCommon if sender.victim is None else self._commandVictim)(sender, args)
                        except Exception as exc:
                            sender.append(OutputLevel.Error, 'Exception occurred while executing command.\n')
                            ExceptionDialog(self, exc)
                        else:
                            if not consumed:
                                sender.append(OutputLevel.Error, 'Unrecognized command; try ":?".\n')
                    elif sender.victim is not None:
                        # Input for victim
                        try:
                            await sender.victim.input(message)
                        except DbgEngException as exc:
                            sender.append(OutputLevel.Error, str(exc) + '\n')
                        except ValueError:
                            # Victim not found, time to close this page
                            self._pages_ctl.forget(sender)
                            del self._pages[sender.victim.pid]

                            if not sender.victim_wait_task.done():
                                sender.victim_wait_task.cancel()
                            try:
                                await sender.victim_wait_task
                            except asyncio.CancelledError:
                                pass

                            sender.destroy()
                    else:
                        # "Input" on server page
                        sender.append(OutputLevel.Error, 'Invalid input; try ":?".\n')
                finally:
                    self._messages.task_done()
        except asyncio.CancelledError:
            raise
        finally:
            if self._client is not None:
                await self._client.close()
                self._client = None

    # --- Extends Toplevel

    def destroy(self) -> None:
        self._messages.put_nowait((None, 'destroy'))
        super().destroy()

