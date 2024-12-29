import asyncio
import tkinter as tk
import tkinter.ttk as ttk

from typing import Optional
from collections import deque

from ._Fonts import FONT_MONOSPACE
from ._ReadonlyText import ReadonlyText
from ..Client import OutputLevel, VictimProxy, BpActionParams, PROMPT_BUSY, STILL_ACTIVE

_PROMPT_SERVER = 'Svr>'

class MainWindowPage(ttk.Frame):
    def __init__(self, master: tk.Widget, victim: Optional[VictimProxy], **kwargs):
        self._victim = victim
        self._victim_ready = False
        self._victim_wait_task = None if victim is None else asyncio.create_task(self._victim_wait())
        self._bp_action: BpActionParams = None
        self._bp_action_cv = asyncio.Condition()
        self._history: deque[str] = deque(maxlen = 64)
        self._history_cursor = 0 # Range: 0 (new line), 1 ~ len(_history)
        self._event_data_input: str = None

        super().__init__(master, **kwargs)
        self.event_add('<<Input>>', tk.NONE)

        font_mono = FONT_MONOSPACE(master.winfo_toplevel())

        # NOTE: This is here above _output to prevent force shrinking
        _input_frm = ttk.Frame(self)
        _input_frm.pack(side = tk.BOTTOM, fill = tk.X)

        _prompt = ttk.Label(_input_frm, font = font_mono, width = 6 + 1, relief = tk.GROOVE)
        _prompt.pack(side = tk.LEFT, fill = tk.Y)
        _prompt.config(text = self.prompt)

        _input = ttk.Entry(_input_frm, font = font_mono)
        _input.pack(fill = tk.X)
        _input.bind('<Up>', self._on_input_up)
        _input.bind('<Down>', self._on_input_down)
        _input.bind('<Return>', self._on_input_return)

        _output = ReadonlyText(self, font = font_mono)
        _output.pack(expand = 1, fill = tk.BOTH)
        _output.tag_config('Normal', foreground = 'black')
        _output.tag_config('Warning', foreground = 'orange')
        _output.tag_config('Error', foreground = 'magenta')
        _output.tag_config('Prompt', foreground = 'gray')

        self._prompt = _prompt
        self._input = _input
        self._output = _output

    async def _victim_wait(self) -> None:
        code = await self._victim.wait()
        await self._victim.close()
        if code == STILL_ACTIVE:
            self.append(OutputLevel.Warning, '\nDetached from the process.\n')
        else:
            self.append(OutputLevel.Warning, f'\nProcess exited with code {code} ({hex(code)}).\n')
        self.append(OutputLevel.Warning, 'Press Enter to close this tab.\n\n')

    def _on_input_up(self, event: tk.Event) -> str:
        cursor = self._history_cursor + 1
        if cursor <= len(self._history):
            self._history_cursor = cursor
            self._input.delete(0, tk.END)
            self._input.insert(0, self._history[-cursor])
        return ''
    
    def _on_input_down(self, event: tk.Event) -> str:
        cursor = self._history_cursor - 1
        if cursor >= 0:
            self._history_cursor = cursor
            self._input.delete(0, tk.END)
            if cursor != 0:
                self._input.insert(0, self._history[-cursor])
        return ''
    
    def _on_input_return(self, event: tk.Event) -> str:
        if self._victim is not None and self._victim.prompt == PROMPT_BUSY:
            self.bell()
            return 'break'

        line = self._input.get()
        self._input.delete(0, tk.END)

        self._history_cursor = 0
        if len(line) > 0 and (len(self._history) == 0 or line != self._history[-1]):
            self._history.append(line)
        
        self.append(OutputLevel.Prompt, self.prompt + ' ')
        self.append(OutputLevel.Normal, line + '\n')

        # XXX: `data` field is broken. Do not use.
        # https://stackoverflow.com/questions/16369947
        #self.event_generate('<<Input>>', data = command)
        self._event_data_input = line
        self.event_generate('<<Input>>')
        return ''

    @property
    def victim(self) -> Optional[VictimProxy]:
        return self._victim

    @property
    def victim_wait_task(self) -> Optional[asyncio.Task]:
        return self._victim_wait_task

    @property
    def prompt(self) -> str:
        return _PROMPT_SERVER if self._victim is None else self._victim.prompt

    @prompt.setter
    def prompt(self, value: str) -> None:
        self._prompt.config(text = value)

    @property
    def event_data_input(self) -> str:
        return self._event_data_input

    def focus_input(self) -> None:
        self._input.focus()

    def append(self, level: OutputLevel, message: str) -> None:
        on_buttom = self._output.yscroll[1] == 1.0
        self._output.insert(tk.END, message, level.name)
        if on_buttom:
            self._output.yscroll = 1.0

    def clear(self) -> None:
        self._output.delete('1.0', tk.END)

    async def wait_bp_action(self) -> BpActionParams:
        async with self._bp_action_cv:
            await self._bp_action_cv.wait()
            return self._bp_action

    async def notify_bp_action(self, value: BpActionParams) -> None:
        if self.victim is not None and not self._victim_ready:
            await self._victim.ready()
            self._victim_ready = True
        else:
            async with self._bp_action_cv:
                self._bp_action = value
                self._bp_action_cv.notify_all()

