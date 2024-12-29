import tkinter as tk

from ._TextDialog import TextDialog
from .._Util.Exception import format_exception

class ExceptionDialog(TextDialog):
    def __init__(self, parent: tk.Toplevel, exc: Exception, **kwargs) -> None:
        parent.bell() # Beep once cuz why not
        super().__init__(
            parent,
            title = 'Exception occurred',
            content = format_exception(exc),
            width = 80,
            height = 12,
            **kwargs
        )

