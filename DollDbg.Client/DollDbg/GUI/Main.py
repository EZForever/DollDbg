import asyncio
import inspect

from ._AsyncTk import AsyncTk
from ._MainWindow import MainWindow
from ._TextDialog import TextDialog
from ._ExceptionDialog import ExceptionDialog

_USAGE = inspect.cleandoc('''
    usage: python -m DollDbg.GUI [connection_string]

    where connection_string:
        tcp:<host>[:<port>]
        npipe:\\\\.\\pipe\\<pipe_name>
''')

def _usage_dialog(root: AsyncTk) -> None:
    TextDialog(root, 'DollDbg GUI Usage', _USAGE, width = 80, height = 12)

async def _await_cancel(task: asyncio.Task) -> None:
    task.cancel()
    try:
        await task
    except asyncio.CancelledError:
        pass

async def amain(args: list[str]) -> int:
    '''
        Async entrypoint for DollDbg GUI.
    '''
    root = AsyncTk()
    root.withdraw()
    root_task = asyncio.create_task(root.amainloop())

    win = None
    win_task = None
    try:
        if len(args) > 2:
            _usage_dialog(root)
            return 1

        win = MainWindow(root)
        win_task = asyncio.create_task(win.amsgloop(args[1] if len(args) > 1 else None))
        done, _ = await asyncio.wait([root_task, win_task], return_when = asyncio.FIRST_COMPLETED)
        for task in done:
            exc = task.exception()
            if exc is not None:
                raise RuntimeError('Uncaught exception in event loop') from exc
    except Exception as exc:
        if not root_task.done():
            if win is not None:
                win.withdraw()
            ExceptionDialog(root, exc)
        return 1
    finally:
        if win_task is not None and not win_task.done():
            await _await_cancel(win_task)
        if not root_task.done():
            await _await_cancel(root_task)
        root.destroy()
    
    return 0

