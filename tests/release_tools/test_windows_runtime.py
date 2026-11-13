"""DLL packaging fails closed and follows transitive PE dependencies."""
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'scripts'))
from windows_runtime import runtime_files

class WindowsRuntimeTests(unittest.TestCase):
 def test_transitive_dlls_and_windows_system_imports(self):
  with tempfile.TemporaryDirectory() as folder:
   root=Path(folder)
   for name in ['game.exe','SDL2.dll','helper.dll']:(root/name).write_bytes(b'PE fixture')
   def inspect(args,**kwargs):
    imports={'game.exe':['KERNEL32.dll','SDL2.dll'],'SDL2.dll':['api-ms-win-crt-runtime-l1-1-0.dll','helper.dll'],'helper.dll':['USER32.dll']}[Path(args[-1]).name]
    return subprocess.CompletedProcess(args,0,'file format pei-x86-64\n'+''.join(' DLL Name: '+name+'\n' for name in imports),'')
   with patch('windows_runtime.subprocess.run',side_effect=inspect):
    files=runtime_files([root/'game.exe'],[root],objdump='fixture-objdump')
   self.assertEqual({p.name for p in files},{'SDL2.dll','helper.dll'})
 def test_missing_dependency_rejected(self):
  result=subprocess.CompletedProcess([],0,'file format pei-x86-64\nDLL Name: missing-lineclash.dll','')
  with patch('windows_runtime.subprocess.run',return_value=result):
   with self.assertRaisesRegex(ValueError,'Missing Windows runtime DLL'):runtime_files([Path('game.exe')],objdump='fixture')
 def test_wrong_architecture_rejected(self):
  with patch('windows_runtime.subprocess.run',return_value=subprocess.CompletedProcess([],0,'file format elf64-x86-64','')):
   with self.assertRaisesRegex(ValueError,'Windows x64'):runtime_files([Path('game.exe')],objdump='fixture')
