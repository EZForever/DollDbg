import sys
import asyncio
from DollDbg.Client import *

FORMAT_MAPPING = {
    OutputLevel.Normal: '\033[97m',
    OutputLevel.Warning: '\033[93m',
    OutputLevel.Error: '\033[95m'
}
FORMAT_RESET = '\033[39m'

async def amain(args: list[str]) -> int:
    async with Client(r'tcp:localhost:31415') as client:
        @client.output
        async def onOutput(_, level: OutputLevel, message: str) -> None:
            if level == OutputLevel.Prompt:
                return
            print(FORMAT_MAPPING[level] + message + FORMAT_RESET, end = '')

        async with await client.launch(None, r'cmd.exe') as victim:
            print('victim.pid =', hex(victim.pid))

            await victim.loadSymbol('kernelbase')

            print('CreateProcessW =', hex(await victim.eval('CreateProcessW')))

            @victim.bp('CreateProcessW')
            async def onCreateProcessW(_, *args) -> BpActionParams:
                await victim.input('.echo CreateProcessW breakpoint hit')
                await victim.input('du @rcx; du @rdx')
                return BpAction.Skip(return_value = 0)

            await victim.ready()
            await victim.wait()
        
    return 0

if __name__ == '__main__':
    exit(asyncio.run(amain(sys.argv)))

