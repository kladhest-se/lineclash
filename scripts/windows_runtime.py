"""Resolve non-system PE imports; never silently ship a ZIP with missing DLLs."""
from pathlib import Path
import re
import shutil
import subprocess
import sys

SYSTEM_DLLS=set("advapi32 bcrypt cfgmgr32 comctl32 comdlg32 crypt32 d3d9 d3d11 dinput8 dsound dwmapi dxgi gdi32 hid imm32 iphlpapi kernel32 msvcrt ntdll ole32 oleaut32 powrprof psapi rpcrt4 secur32 setupapi shell32 shlwapi ucrtbase user32 userenv usp10 version winhttp winmm winspool ws2_32 wtsapi32".split())

def runtime_files(executables,search_dirs=(),objdump=None):
    objdump=objdump or shutil.which('objdump')
    if not objdump:raise ValueError('Windows packaging requires objdump (MSYS2 binutils)')
    folders=[Path(p) for p in search_dirs]+[Path(sys.executable).parent]
    compiler=shutil.which('gcc')
    if compiler:folders.append(Path(compiler).parent)
    index={}
    for folder in folders:
        if folder.is_dir():
            for path in folder.iterdir():
                if path.suffix.lower()=='.dll' and path.is_file():index.setdefault(path.name.lower(),path.resolve())
    pending=list(map(Path,executables));seen=set();dlls={}
    while pending:
        path=pending.pop()
        key=str(path.resolve()).lower()
        if key in seen:continue
        seen.add(key)
        result=subprocess.run([str(objdump),'-p',str(path)],check=True,text=True,capture_output=True)
        if 'pei-x86-64' not in result.stdout:raise ValueError('Expected Windows x64 PE file: '+str(path))
        for name in re.findall(r'DLL Name:\s*(\S+)',result.stdout):
            name=name.lower()
            if name.removesuffix('.dll') in SYSTEM_DLLS or name.startswith(('api-ms-win-','ext-ms-win-')):continue
            if name not in index:raise ValueError('Missing Windows runtime DLL: '+name)
            if name not in dlls:dlls[name]=index[name];pending.append(index[name])
    return sorted(dlls.values(),key=lambda p:p.name.lower())
