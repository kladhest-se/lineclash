#!/usr/bin/env python3
"""Real PHP/PDO/MariaDB checks. Requires a disposable database in WEB_CONFIG."""
import base64
import gzip
import hashlib
import json
import os
from pathlib import Path
import socket
import subprocess
import tempfile
import time
import urllib.request
import urllib.error
import uuid

root=Path(__file__).resolve().parents[1]
assert os.environ.get('LINECLASH_WEB_CONFIG'), 'Set LINECLASH_WEB_CONFIG to an empty disposable MariaDB database config'
php=os.environ.get('PHP','php')
subprocess.run([php,str(root/'public-web/bin/manage.php'),'migrate'],check=True)
id='test_'+uuid.uuid4().hex
out=subprocess.check_output([php,str(root/'public-web/bin/manage.php'),'server',id,'127.0.0.1','31457','<Test & server>'],text=True)
token=out.strip().split()[-1]
with socket.socket() as s: s.bind(('127.0.0.1',0)); port=s.getsockname()[1]
base=f'http://127.0.0.1:{port}'
with tempfile.TemporaryFile() as log:
    p=subprocess.Popen([php,'-S',f'127.0.0.1:{port}','-t',str(root/'public-web'),str(root/'public-web/dev-router.php')],stdout=log,stderr=log)
    def call(path='/api/',payload=None):
        req=urllib.request.Request(base+path,json.dumps(payload).encode() if payload is not None else None,{'Content-Type':'application/json'})
        try:
            with urllib.request.urlopen(req,timeout=3) as response: return response.status,response.read()
        except urllib.error.HTTPError as e: return e.code,e.read()
    try:
        for _ in range(100):
            try: call(); break
            except OSError: time.sleep(.05)
        payload=dict(server_id=id,token=token,database_id=uuid.uuid4().hex,status={'players':2,'rooms':1},games=[dict(game_id=2,room_id=1,started_at='2026-11-13 10:00:00',ended_at='2026-11-13 10:05:00',winner_nick='<Winner>',winner_team='',player_count=2)])
        g=payload['games'][0]
        g.update(started_epoch=1794564000,ended_epoch=1794564300,ranked=1,end_reason='completed',players=[dict(nick='<Winner>',placement=1,won=1,lines_cleared=12,specials_sent=3,blocks_placed=50)])
        raw=json.dumps([dict(elapsed_ms=0,line='newgame 1'),dict(elapsed_ms=100,line='f 1 '+'0'*264),dict(elapsed_ms=200,line='endgame')],separators=(',',':')).encode()
        g.update(replay_gzip=base64.b64encode(gzip.compress(raw)).decode(),replay_sha256=hashlib.sha256(raw).hexdigest(),replay_truncated=0)
        bad=dict(payload,token='wrong'); assert call(payload=bad)[0]==403
        assert call(payload=payload)[0]==200
        assert call(payload=payload)[0]==200
        payload['games'][0].update(game_id=1,ranked=0,end_reason='stopped')
        assert json.loads(call(payload=payload)[1])['ack']==[1]
        bad=dict(payload,games=[dict(payload['games'][0],player_count=-1)])
        assert call(payload=bad)[0]==400
        rows=json.loads(call('/api/?action=matches')[1])['matches']
        assert len([g for g in rows if g['server_id']==id])==2
        assert any(s['id']==id for s in json.loads(call()[1])['servers'])
        page=call('/directory/')[1].decode(); assert '&lt;Winner&gt;' in page and '<Winner>' not in page
        assert call('/inc/schema.sql')[0] in (403,404)
        assert call('/config.example.php')[0] in (403,404)
        assert call('/api/?action=matches&offset=-1')[0]==400
        from urllib.parse import urlencode
        key=urlencode(dict(server=id,database=payload['database_id'],game=2))
        replay=json.loads(call('/api/?action=replay&'+key)[1])
        assert replay['match']['ranked']==1 and replay['players'][0]['lines_cleared']==12
        assert hashlib.sha256(replay['replay_source'].encode()).hexdigest()==replay['match']['replay_sha256']
        assert len(replay['events'])==3
        leaders=json.loads(call('/api/?action=leaders')[1])['leaders']
        assert next(x for x in leaders if x['server_id']==id)['wins']==1
        history=json.loads(call('/api/?action=player&'+urlencode(dict(server=id,nick='<Winner>')))[1])['matches']
        assert len(history)==2
        assert len(json.loads(call('/api/?action=matches&server='+id+'&mode=ranked')[1])['matches'])==1
        assert call('/api/?format=text')[1].startswith(b'LC-DIRECTORY-1\n')
        bad=dict(payload,games=[dict(g,game_id=3,replay_sha256='0'*64)])
        assert call(payload=bad)[0]==400
        bad=dict(payload,games=[dict(g,game_id=3,ranked=1,end_reason='stopped')])
        assert call(payload=bad)[0]==400
        assert call(payload=dict(payload,games=[dict(g,game_id=3,players=[1])]))[0]==400
        invalid_raw=b'[1]'
        invalid=dict(g,game_id=3,replay_gzip=base64.b64encode(gzip.compress(invalid_raw)).decode(),replay_sha256=hashlib.sha256(invalid_raw).hexdigest())
        assert call(payload=dict(payload,games=[invalid]))[0]==400
        for path in ['/', '/servers/', '/leaderboards/', '/matches/', '/statistics/', '/directory/']:
            code,body=call(path);assert code==200,(path,code,body[:300])
            text=body.decode();assert 'aria-label="Main"' in text and 'Leaderboards' in text and 'assets/site.css' in text,path
        home=call('/')[1].decode();assert 'Live servers' in home and 'On the leaderboard' in home and 'Fresh from the arena' in home
        assert '&lt;Winner&gt;' in home and '<Winner>' not in home
        servers=call('/servers/?q='+id)[1].decode()
        assert '0 live servers in this view' in servers
        assert call('/servers/?availability=invalid')[0]==400
        assert call('/matches/?mode=invalid')[0]==400
        assert call('/matches/?offset=-1')[0]==400
        filtered=call('/leaderboards/?server='+id)[1].decode();assert '&lt;Winner&gt;' in filtered
        ranked=call('/matches/?server='+id+'&mode=ranked')[1].decode();assert 'Match #2' in ranked and 'Match #1<' not in ranked
        assert 'Player results' in call('/directory/match.php?'+key)[1].decode()
        assert 'A local rivalry' in call('/directory/player.php?'+urlencode(dict(server=id,nick='<Winner>')))[1].decode()
        # Bounded pagination preserves server/mode filters and escapes display names.
        extra=[]
        for i in range(100,151):
            extra.append(dict(game_id=i,room_id=1,started_at='2026-11-13 10:00:00',ended_at='2026-11-13 10:05:00',winner_nick='',winner_team='',player_count=1))
        assert call(payload=dict(payload,games=extra))[0]==200
        first=call('/matches/?server='+id+'&mode=casual')[1].decode()
        assert 'Next' in first and 'mode=casual' in first and 'offset=50' in first
        second=call('/matches/?server='+id+'&mode=casual&offset=50')[1].decode();assert 'Previous' in second and 'Page 2' in second
        assert 'Copy address' in call('/servers/')[1].decode()
        assert call(payload=dict(payload,games=[],status={}))[0]==200
        unknown=call('/servers/?availability=open&q=Test')[1].decode();assert '&lt;Test &amp; server&gt;' not in unknown
        print('PASS: integrated homepage, shared navigation, full community pages, HTML escaping, filters and pagination')
        print('PASS: ranked/casual filters, player counters/history, replay integrity and native discovery feed')
        print('PASS: MariaDB migration, provisioning, auth, live listing, deduplication, out-of-order delivery, validation, HTML escaping and private routes')
    finally:
        p.terminate(); p.wait(timeout=5)
