#!/usr/bin/env python3
import os
from pathlib import Path
import socket
import subprocess
import tempfile
import time
import urllib.error
import urllib.request
root=Path(__file__).resolve().parents[1]
with socket.socket() as s:s.bind(('127.0.0.1',0));port=s.getsockname()[1]
with tempfile.TemporaryDirectory() as folder, tempfile.TemporaryFile() as log:
 env=dict(os.environ,LINECLASH_WEB_CONFIG=str(Path(folder)/'missing.php'))
 p=subprocess.Popen([os.environ.get('PHP','php'),'-S',f'127.0.0.1:{port}','-t',str(root/'public-web'),str(root/'public-web/dev-router.php')],env=env,stdout=log,stderr=log)
 def get(path):
  try:
   with urllib.request.urlopen(f'http://127.0.0.1:{port}'+path,timeout=2) as response:return response.status,response.read().decode()
  except urllib.error.HTTPError as e:return e.code,e.read().decode()
 try:
  for _ in range(100):
   try:code,home=get('/');break
   except OSError:time.sleep(.05)
  else:raise AssertionError('PHP preview failed to start')
  assert code==200 and 'Community data is temporarily unavailable' in home and 'Get the game' in home
  assert '0 live servers' not in home
  for path in ['/servers/','/leaderboards/','/matches/','/statistics/']:
   code,body=get(path);assert code==503 and 'aria-label="Main"' in body and 'temporarily unavailable' in body,path
  assert get('/assets/site.css')[0]==200
  assert get('/config.inc.php')[0] in (403,404)
  print('PASS: homepage survives unavailable database; community pages show honest unavailable states with navigation')
 finally:p.terminate();p.wait(timeout=5)
