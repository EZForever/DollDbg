#!/usr/bin/env python3
import sys
import asyncio

from .Main import amain

if __name__ == '__main__':
    sys.exit(asyncio.run(amain(sys.argv)))

