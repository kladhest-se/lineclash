import hashlib
from test_protocol import Peer
import os,subprocess,pathlib,tempfile,socket,time,json,urllib.request,sqlite3,uuid
r=pathlib.Path(__file__).resolve().parents[1];php=os.environ.get('PHP','php')
def port():
 with socket.socket() as s:s.bind(('127.0.0.1',0));return s.getsockname()[1]
assert os.environ.get('LINECLASH_WEB_CONFIG'), 'Set LINECLASH_WEB_CONFIG to a disposable MariaDB database config'
env=dict(os.environ)
id='supervisor_'+uuid.uuid4().hex;token=subprocess.check_output([php,str(r/'public-web/bin/manage.php'),'server',id,'127.0.0.1','31457','Supervisor Test'],env=env,text=True).strip().split()[-1]
env['LINECLASH_DIRECTORY_TOKEN']=token
with tempfile.TemporaryDirectory() as d:
 d=pathlib.Path(d);webport,gameport,apiport=port(),port(),port();url=f'http://127.0.0.1:{webport}/api/'
 cfg=d/'server.cfg';cfg.write_text(f'port={gameport}\nstatus_api=on\nstatus_port={apiport}\ndb_file={d}/games.sqlite3\nlog_file={d}/game.log\n')
 with (d/'web.log').open('w') as wl,(d/'supervisor.log').open('w') as sl:
  web=subprocess.Popen([php,'-S',f'127.0.0.1:{webport}','-t',str(r/'public-web'),str(r/'public-web/dev-router.php')],env=env,stdout=wl,stderr=wl)
  p=None
  try:
   time.sleep(.3)
   p=subprocess.Popen(['python3',str(r/'scripts/server_directory.py'),'--url',url,'--server-id',id,'--database',str(d/'games.sqlite3'),'--state',str(d/'delivery.sqlite3'),'--interval','15','--status-url',f'http://127.0.0.1:{apiport}/status','--allow-local-http','--',str(r/'server/lineclashd'),'--config',str(cfg)],env=env,stdin=subprocess.PIPE,stdout=sl,stderr=sl)
   deadline=time.monotonic()+8
   while not (d/'games.sqlite3').exists() and time.monotonic()<deadline:time.sleep(.05)
   time.sleep(.5)
   first,second=Peer(gameport,'TestWinner'),Peer(gameport,'TestLoser')
   try:
    first.send('clientinfo gui test 1.0.0 input-v1');second.send('clientinfo gui test 1.0.0 input-v1')
    first.join();second.join();first.send('playerready');second.send('playerready');first.sync();second.sync()
    first.send('startgame 1');first.until('newgame');second.until('newgame')
    second.until('statev1 ')
    for seq in range(1,100):
     second.send(f'inputv1 {seq} drop');time.sleep(.06)
     with sqlite3.connect(d/'games.sqlite3') as db:
      if db.execute('SELECT ended_epoch FROM games ORDER BY game_id DESC LIMIT 1').fetchone()[0]:break
    first.until('endgame');first.sync()
   finally:first.close();second.close()
   deadline=time.monotonic()+22
   while time.monotonic()<deadline:
    with urllib.request.urlopen(url+'?action=matches') as resp:rows=json.load(resp)['matches']
    if any(x['server_id']==id for x in rows):break
    if p.poll() is not None:raise AssertionError((d/'supervisor.log').read_text())
    time.sleep(.2)
   else:raise AssertionError('Game never delivered')
   with urllib.request.urlopen(url) as resp:servers=json.load(resp)['servers']
   row=next(x for x in servers if x['id']==id);assert row['status']['players']==0 and row['status']['rooms']>=1,row
   game=next(x for x in rows if x['server_id']==id)
   assert int(game['ranked'])==1 and int(game['started_epoch'])>0,game
   from urllib.parse import urlencode
   query=urlencode(dict(action='replay',server=id,database=game['database_id'],game=game['game_id']))
   with urllib.request.urlopen(url+'?'+query) as response:replay=json.load(response)
   assert len(replay['players'])==2 and replay['events'],replay
   assert hashlib.sha256(replay['replay_source'].encode()).hexdigest()==replay['match']['replay_sha256']
   replay_path=d/'replay.json';replay_path.write_text(json.dumps(replay))
   subprocess.run(['python3',str(r/'scripts/verify_replay.py'),str(replay_path)],check=True)
   health_path=d/'delivery.sqlite3.health.json';health=json.loads(health_path.read_text())
   assert health['last_success']>0 and token not in health_path.read_text(),health
   p.terminate();p.wait(timeout=8);p.stdin.close()
   try:
    with socket.create_connection(('127.0.0.1',gameport),timeout=.3):raise AssertionError('Child still running')
   except OSError:pass
   print('PASS: supervisor starts real server, announces live counts, exports a played ranked round, linked players and a verified replay to MariaDB and stops its child cleanly')
  finally:
   if p and p.poll() is None:p.terminate();p.wait(timeout=8)
   web.terminate();web.wait(timeout=5)
