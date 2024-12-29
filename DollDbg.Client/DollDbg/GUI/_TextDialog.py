import tkinter as tk
import tkinter.ttk as ttk

from tkinter.simpledialog import Dialog

from ._Fonts import FONT_MONOSPACE
from ._ReadonlyText import ReadonlyText

class TextDialog(Dialog):
    def __init__(self, parent: tk.Toplevel, title: str, content: str, **kwargs) -> None:
        self._content = content
        self._kwargs = kwargs

        # Calls body() and buttonbox() and waits
        super().__init__(parent, title)

    # --- Extends Dialog

    def body(self, master: tk.Toplevel) -> None:
        font_mono = FONT_MONOSPACE(master)

        self.resizable(width = False, height = False) 

        self._text = ReadonlyText(master, font = font_mono, **self._kwargs)
        self._text.pack(expand = 1, fill = tk.BOTH)
        self._text.insert(tk.END, self._content)

    def buttonbox(self) -> None:
        self.bind("<Return>", self.ok)
        self.bind("<Escape>", self.cancel)

        self._close = ttk.Button(self, text = 'Close', underline = 0, command = self.ok)
        self._close.pack(fill = tk.X)
        self._close.focus()

