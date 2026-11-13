#!/usr/bin/env python3
"""Extract a package and check startup with no development DLL directories."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import tarfile
from zipfile import ZipFile

archive=Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix='lineclash-package-') as folder:
 root=Path(folder)
 if archive.name.endswith('.tar.gz'):
  with tarfile.open(archive) as z:
   for item in z.getmembers():
    dest=(root/item.name).resolve()
    if not dest.is_relative_to(root) or not item.isfile():raise ValueError('Unsafe archive member')
   z.extractall(root)
 else:
  with ZipFile(archive) as z:
   for item in z.infolist():
    dest=(root/item.filename).resolve()
    if not dest.is_relative_to(root):raise ValueError('Unsafe archive member')
   z.extractall(root)
 suffix='.exe' if os.name=='nt' else ''
 env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy')
 if os.name=='nt':
  windows=Path(env.get('SystemRoot',r'C:\Windows'))
  env['PATH']=str(windows/'System32')+os.pathsep+str(windows)
 for name,args in [('lineclashd',['--version']),('lineclash',['--smoke-test'])]:
  exe=root/'bin'/(name+suffix)
  if os.name!='nt':exe.chmod(0o755)
  subprocess.run([str(exe)]+args,cwd=root,env=env,check=True,timeout=20)
 print('PASS: extracted server and game start, SDL/audio initialize, bundled title track loads')
