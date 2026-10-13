import argparse
import asyncio
import logging
from pathlib import Path
from . import __version__
from .config import Config

def main():
    parser=argparse.ArgumentParser(description='Local A2M/16 and C64M/10 MCP server')
    parser.add_argument('--config',help='Absolute TOML configuration path')
    parser.add_argument('--version',action='version',version=__version__)
    parser.add_argument('--log-level',choices=('DEBUG','INFO','WARNING','ERROR'),default='WARNING')
    args=parser.parse_args()
    logging.basicConfig(level=args.log_level)
    from .server import run
    asyncio.run(run(Config.load(args.config) if args.config else Config()))

if __name__=='__main__': main()
