import asyncio
import tkinter as tk

from typing import Optional

class AsyncTk(tk.Tk):
    def __init__(self, **kwargs):
        super().__init__(**kwargs)
    
    async def amainloop(self, interval: float = 1 / 30) -> None:
        try:
            while True:
                self.update()
                await asyncio.sleep(interval)
        except asyncio.CancelledError:
            raise

    async def await_window(self, window: Optional[tk.Toplevel] = None, interval: float = 1 / 30) -> None:
        if window is None:
            window = self
        try:
            while window.winfo_exists():
                await asyncio.sleep(interval)
        except asyncio.CancelledError:
            raise

