import traceback

def format_exception(exc: BaseException) -> str:
    exc_type = type(exc)
    exc_tb = exc.__traceback__
    if exc_tb is None:
        return ''.join(traceback.format_exception_only(exc_type, exc))
    else:
        return ''.join(traceback.format_exception(exc_type, exc, exc_tb))

