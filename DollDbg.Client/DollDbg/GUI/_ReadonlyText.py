import string
import tkinter as tk
import tkinter.ttk as ttk

from typing import Optional

# HACK: Why 0x0008 (Alt on document) is always set while 0x20000 is the true state?
_KEY_WHITELIST = {
    # Direction keys w/ optional Shift
    'Up'    : [0x0000, 0x0001],
    'Down'  : [0x0000, 0x0001],
    'Left'  : [0x0000, 0x0001],
    'Right' : [0x0000, 0x0001],
    'Home'  : [0x0000, 0x0001],
    'End'   : [0x0000, 0x0001],
    'Prior' : [0x0000, 0x0001],
    'Next'  : [0x0000, 0x0001],
    # Ctrl-A, Ctrl-C, Alt-A and Alt-C
    'A'     : [0x0004, 0x20000],
    'a'     : [0x0004, 0x20000],
    'C'     : [0x0004, 0x20000],
    'c'     : [0x0004, 0x20000],
    # Tab, Shift-Tab and Enter gets special treatment
    # Ctrl-Tab & Ctrl-Shift-Tab is also allowed w/o treatment
    'Tab'   : [0x0000, 0x0001, 0x0004],
    'Return': [0x0000],
    # Alt-F4
    'F4'    : [0x20000],
} | {
    # Alt-x switches tabs (x ~= 0-9A-Za-z except A & C)
    x       : [0x20000] \
        for x in string.digits + string.ascii_letters \
        if x not in 'AaCc'
}

class ReadonlyText(tk.Text):
    def __init__(self, master: Optional[tk.Widget] = None, **kwargs):
        self._frame = ttk.Frame(master)
        self._frame.grid_rowconfigure(0, weight = 1)
        self._frame.grid_columnconfigure(0, weight = 1)

        super().__init__(self._frame, wrap = tk.NONE, undo = False, **kwargs)
        self.grid(row = 0, column = 0, sticky = tk.NSEW)
        self.bind('<Key>', self._on_key)

        self._xscroll = ttk.Scrollbar(self._frame, orient = tk.HORIZONTAL, command = self.xview)
        self._xscroll.grid(row = 1, column = 0, sticky = tk.EW)

        self._yscroll = ttk.Scrollbar(self._frame, orient = tk.VERTICAL, command = self.yview)
        self._yscroll.grid(row = 0, column = 1, sticky = tk.NS)

        self._filler = ttk.Label(self._frame, relief = tk.GROOVE)
        self._filler.grid(row = 1, column = 1, sticky = tk.NSEW)

        self.config(xscrollcommand = self._xscroll.set, yscrollcommand = self._yscroll.set)

        # HACK: Copy geometry methods of self._frame without overriding Text methods
        # Copied from tkinter.scrolledtext
        text_methods = vars(tk.Text).keys()
        methods = vars(tk.Pack).keys() | vars(tk.Grid).keys() | vars(tk.Place).keys()
        methods = methods.difference(text_methods)

        for m in methods:
            if m[0] != '_' and m != 'config' and m != 'configure':
                setattr(self, m, getattr(self._frame, m))

    def __str__(self) -> str:
        return str(self._frame)

    def _on_key(self, event: tk.Event) -> str:
        state = event.state & ~0x40008
        if event.keysym not in _KEY_WHITELIST \
            or state not in _KEY_WHITELIST[event.keysym]:
            return 'break'

        if event.keysym == 'Tab' and (state & 0x0004) == 0:
            # https://stackoverflow.com/a/6781700
            cmd = 'tk_focusNext' if (state & 0x0001) == 0 else 'tk_focusPrev'
            if widget := self.tk.call(cmd, self._w):
                self.winfo_toplevel().nametowidget(widget.string).focus()
            return 'break'
        elif event.keysym == 'Return':
            self.mark_set(tk.INSERT, 'end linestart')
            self.xview_moveto(0.0)
            self.yview_moveto(1.0)
            return 'break'
        
        return ''

    @property
    def xscroll(self) -> tuple[float, float]:
        return self._xscroll.get()
    
    @xscroll.setter
    def xscroll(self, value: float) -> None:
        self.xview_moveto(value)

    @property
    def yscroll(self) -> tuple[float, float]:
        return self._yscroll.get()
    
    @yscroll.setter
    def yscroll(self, value: tuple[float]) -> None:
        self.yview_moveto(value)

