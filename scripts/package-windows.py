#!/usr/bin/env python3
"""Package already-built x64 Windows executables and their runtime DLLs."""
import argparse
from pathlib import Path
from zipfile import ZipFile,ZIP_DEFLATED
from release import native_files
from build_provenance import ROOT,version,digest

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime-dir',type=Path,action='append',default=[],help='Additional trusted DLL folder; repeatable')
    parser.add_argument('--objdump',help='PE-aware objdump (default: objdump from PATH)')
    parser.add_argument('--output',type=Path)
    args=parser.parse_args()
    target=args.output or ROOT/'dist'/('LineClash-'+version()+'-windows-x86_64.zip')
    if target.exists():parser.error('Output already exists; preserve it or select a new --output')
    entries=native_files(ROOT,windows=True,runtime_dirs=args.runtime_dir,objdump=args.objdump)
    for path,name in entries:digest(ROOT/path)
    target.parent.mkdir(parents=True,exist_ok=True)
    with ZipFile(target,'x',compression=ZIP_DEFLATED) as archive:
        for path,name in entries:archive.write(ROOT/path,name)
    print(str(target))
    print('Windows x64 binaries and required non-system DLLs included. Run make verify on Windows before release.')
if __name__=='__main__':main()
