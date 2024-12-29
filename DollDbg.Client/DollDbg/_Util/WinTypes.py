from win32file import AllocateReadBuffer
from pywintypes import HANDLE, OVERLAPPED

PyHANDLE = type(HANDLE())
PyOVERLAPPED = type(OVERLAPPED())
PyOVERLAPPEDReadBuffer = type(AllocateReadBuffer(0)) # Actually a PyMemoryView to bytearray

