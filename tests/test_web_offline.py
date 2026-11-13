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
   with urllib.request.urlopen(f'http://127.0.0.1:{port}'+path,timeout=2) as response:return response.status,response.read().decode(errors='replace')
  except urllib.error.HTTPError as e:return e.code,e.read().decode()
 try:
  for _ in range(100):
   try:code,home=get('/');break
   except OSError:time.sleep(.05)
  else:raise AssertionError('PHP preview failed to start')
  assert code==200 and 'Community data is temporarily unavailable' not in home and 'id="screenshots"' in home and 'Get the game' in home
  assert '0 live servers' not in home
  assert 'id="get"' in home and '>Screenshots</a>' not in home
  for path,heading in [('/documentation/','Guides for players and server operators.'),('/documentation/basics/','Join'),('/documentation/controls/','Default desktop controls'),('/documentation/features/','Servers, match history and replays'),('/documentation/modes/','Game modes'),('/documentation/specials/','Specials'),('/documentation/server/','Configure a game server'),('/source/','Build the server.'),('/credits/','The people behind LineClash.'),('/privacy/','Public game information'),('/api/docs/','Public endpoints')]:
   code,body=get(path);assert code==200 and heading in body and 'id="block-background"' in body,path
   assert 'aria-label="Main"' in body and '#get"' in body
   if path.startswith('/documentation/') and path!='/documentation/':assert '<aside' not in body,path
  assert 'href="./" aria-current="page">Home</a>' in home
  for path in ['/servers/','/leaderboards/','/matches/','/statistics/']:
   code,body=get(path);assert code==503 and 'aria-label="Main"' in body and 'temporarily unavailable' in body,path
  assert get('/assets/site.css')[0]==200
  for asset in ['blocks.js','downloads.js','screenshots/game.png','screenshots/servers.png']:assert get('/assets/'+asset)[0]==200
  assert get('/config.inc.php')[0] in (403,404)
  print('PASS: homepage survives unavailable database; community pages show honest unavailable states with navigation')
 finally:p.terminate();p.wait(timeout=5)
