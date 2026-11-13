#!/usr/bin/env python3
"""Supervise a dedicated server and deliver public results without blocking gameplay.
Python 3.10+, standard library only. Tokens are read from the environment.
"""
import argparse
import base64
import gzip
import hashlib
import json
import os
from pathlib import Path
import signal
import sqlite3
import subprocess
import sys
import threading
import time
import urllib.parse
import urllib.request
import uuid


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        return None


def endpoint(url, local=False):
    p = urllib.parse.urlsplit(url)
    if p.username or p.password or p.fragment or not p.hostname:
        raise ValueError('Invalid directory URL')
    if p.scheme != 'https' and not (local and p.scheme == 'http' and p.hostname in ('127.0.0.1', '::1', 'localhost')):
        raise ValueError('Use HTTPS (or explicitly allow loopback HTTP for development)')
    return url


def read_games(path, state=None, destination=None):
    uri = Path(path).resolve().as_uri() + '?mode=ro'
    with sqlite3.connect(uri, uri=True, timeout=1) as db:
        db.row_factory = sqlite3.Row
        db.execute("BEGIN")
        condition = ""
        parameters = ()
        if state is not None:
            db.execute('ATTACH DATABASE ? AS delivery', (Path(state).resolve().as_uri()+'?mode=ro',))
            condition = ' AND NOT EXISTS (SELECT 1 FROM delivery.delivered d WHERE d.game_id=games.game_id AND d.destination=?)'
            parameters = (destination,)
        columns={row[1] for row in db.execute('PRAGMA table_info(games)')}
        extra=['started_epoch','ended_epoch','ranked','end_reason','replay_truncated']
        fields=','.join(k if k in columns else ("'legacy'" if k=='end_reason' else '0')+' AS '+k for k in extra)
        games=[dict(row) for row in db.execute(
            "SELECT game_id,room_id,started_at,ended_at,winner_nick,winner_team,player_count," + fields + " FROM games WHERE ended_at<>''" + condition + " ORDER BY game_id LIMIT 20", parameters)]
        tables={row[0] for row in db.execute("SELECT name FROM sqlite_master WHERE type='table'")}
        size=0; batch=[]
        for g in games:
            g['players']=[]
            if 'game_stats' in tables and 'game_id' in {row[1] for row in db.execute('PRAGMA table_info(game_stats)')}:
                g['players']=[dict(row) for row in db.execute('SELECT nick,lines_cleared,specials_sent,blocks_placed,placement,won FROM game_stats WHERE game_id=? ORDER BY stat_id LIMIT 8',(g['game_id'],))]
            if 'replay_events' in tables:
                events=[dict(row) for row in db.execute('SELECT elapsed_ms,line FROM replay_events WHERE game_id=? ORDER BY seq LIMIT 2048',(g['game_id'],))]
                if events:
                    raw=json.dumps(events,separators=(',',':')).encode()
                    g['replay_gzip']=base64.b64encode(gzip.compress(raw,mtime=0)).decode()
                    g['replay_sha256']=hashlib.sha256(raw).hexdigest()
            n=len(json.dumps(g).encode())
            if size+n>1500000 and batch: break
            if n>1500000: raise ValueError('Replay exceeds delivery bound')
            size+=n; batch.append(g)
        return batch



class Delivery:
    def __init__(self, state, url, server_id):
        self.db = sqlite3.connect(state)
        self.db.execute('CREATE TABLE IF NOT EXISTS identity(id TEXT NOT NULL)')
        if not self.db.execute('SELECT id FROM identity').fetchone():
            self.db.execute('INSERT INTO identity VALUES(?)', (uuid.uuid4().hex,))
        self.identity = self.db.execute('SELECT id FROM identity').fetchone()[0]
        self.url, self.server_id = url, server_id
        self.destination = hashlib.sha256((url + chr(0) + server_id).encode()).hexdigest()
        self.db.execute('CREATE TABLE IF NOT EXISTS delivered(destination TEXT,game_id INTEGER,PRIMARY KEY(destination,game_id))')
        self.db.commit()

    def bind(self, path):
        # Identity belongs to gameplay data, so deleting delivery state is harmless.
        with sqlite3.connect(path,timeout=1) as source:
            source.execute('CREATE TABLE IF NOT EXISTS service_metadata(key TEXT PRIMARY KEY,value TEXT NOT NULL)')
            source.execute("INSERT OR IGNORE INTO service_metadata VALUES('database_id',?)",(uuid.uuid4().hex,))
            identity=source.execute("SELECT value FROM service_metadata WHERE key='database_id'").fetchone()[0]
            self.db.execute('CREATE TABLE IF NOT EXISTS source_identity(id TEXT NOT NULL)')
            bound=self.db.execute('SELECT id FROM source_identity').fetchone()
            # One-time migration of old delivery state preserves previously published IDs.
            if not bound and self.db.execute('SELECT COUNT(*) FROM delivered').fetchone()[0] and not source.execute("SELECT 1 FROM service_metadata WHERE key='directory_bound'").fetchone():
                identity=self.identity
                source.execute("UPDATE service_metadata SET value=? WHERE key='database_id'",(identity,))
            source.execute("INSERT OR IGNORE INTO service_metadata VALUES('directory_bound','1')")
        old=self.destination
        self.identity=identity
        self.destination=hashlib.sha256((self.url+chr(0)+self.server_id+chr(0)+identity).encode()).hexdigest()
        if not bound:
            self.db.execute('INSERT OR IGNORE INTO delivered SELECT ?,game_id FROM delivered WHERE destination=?',(self.destination,old))
        self.db.execute('DELETE FROM source_identity'); self.db.execute('INSERT INTO source_identity VALUES(?)',(identity,)); self.db.commit()

    def health(self, path, **values):
        values.update(server_id=self.server_id,database_id=self.identity)
        target=Path(path); temporary=target.with_suffix(target.suffix+'.tmp')
        temporary.write_text(json.dumps(values,indent=2)+'\n'); os.chmod(temporary,0o600); temporary.replace(target)

    def batch(self, games):
        done = {r[0] for r in self.db.execute('SELECT game_id FROM delivered WHERE destination=?', (self.destination,))}
        return [g for g in games if g['game_id'] not in done][:100]

    def ack(self, games, response):
        expected = [g['game_id'] for g in games]
        if response.get('ok') is not True or response.get('ack') != expected:
            raise ValueError('Directory did not acknowledge this batch')
        with self.db:
            self.db.executemany('INSERT OR IGNORE INTO delivered VALUES(?,?)', [(self.destination, i) for i in expected])


def send(url, payload):
    request = urllib.request.Request(url, json.dumps(payload).encode(), {'Content-Type': 'application/json'}, method='POST')
    # Default SSL context verifies certificates. Redirects cannot leak the token.
    with urllib.request.build_opener(NoRedirect).open(request, timeout=5) as response:
        data = response.read(2097153)
        if len(data) > 2097152:
            raise ValueError('Oversized directory response')
        return json.loads(data)


def main():
    if len(sys.argv)==3 and sys.argv[1]=='--show-health':
        health=json.loads(Path(sys.argv[2]).read_text())
        print(json.dumps(health,indent=2));return 0
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--url', required=True, help='Full website /api/ URL')
    p.add_argument('--server-id', required=True)
    p.add_argument('--database', required=True, help='Same db_file as the dedicated server')
    p.add_argument('--state', required=True, help='Private durable delivery SQLite file; preserve across restarts')
    p.add_argument('--interval', type=int, default=60)
    p.add_argument('--status-url', help='Optional loopback status API, e.g. http://127.0.0.1:31458/status')
    p.add_argument('--health', help='Private health JSON file; default STATE.health.json')
    p.add_argument('--allow-local-http', action='store_true')
    p.add_argument('command', nargs=argparse.REMAINDER, help='-- /path/to/lineclashd --config /path/to/server.cfg')
    a = p.parse_args()
    try:
        endpoint(a.url, a.allow_local_http)
        if a.status_url:
            endpoint(a.status_url, True)
            if urllib.parse.urlsplit(a.status_url).hostname not in ('127.0.0.1','::1','localhost'): raise ValueError('Status API must use loopback')
        if a.interval < 15 or a.interval > 120: raise ValueError('Interval must be 15–120 seconds')
        token = os.environ['LINECLASH_DIRECTORY_TOKEN']
        if len(token) < 32: raise ValueError('Missing or invalid token')
        if Path(a.state).resolve() == Path(a.database).resolve(): raise ValueError('State and game database must differ')
        if not a.command or a.command[0] != '--' or len(a.command)<2: raise ValueError('Supply dedicated server command after --')
    except (ValueError, KeyError) as e:
        p.error(str(e))
    os.umask(0o077)
    delivery = Delivery(a.state, a.url, a.server_id)
    stop = threading.Event()
    child = subprocess.Popen(a.command[1:])
    def shutdown(signum, frame):
        stop.set()
        if child.poll() is None: child.terminate()
    signal.signal(signal.SIGTERM, shutdown)
    signal.signal(signal.SIGINT, shutdown)
    last_success=0; failures=0
    try:
        previous=json.loads(Path(a.health or a.state+'.health.json').read_text())
        last_success=int(previous.get('last_success',0))
    except (OSError,ValueError,TypeError,AttributeError): pass
    try:
        while child.poll() is None and not stop.is_set():
            try:
                delivery.bind(a.database)
                games = read_games(a.database, a.state, delivery.destination)
                status = {}
                if a.status_url:
                    try:
                        with urllib.request.build_opener(NoRedirect).open(a.status_url, timeout=2) as response:
                            data = response.read(1048577)
                            if len(data)>1048576: raise ValueError('Oversized status')
                            local = json.loads(data)
                            status = dict(players=int(local['players']['online']), rooms=len(local['rooms']), users=int(local['players']['registered']), capacity=sum(int(room.get('max_players',8)) for room in local['rooms']))
                    except Exception:
                        pass  # Status counts are optional; results and heartbeat still deliver.
                payload = dict(server_id=a.server_id, token=token, database_id=delivery.identity, games=games, status=status)
                response = send(a.url, payload)
                delivery.ack(games, response)
                last_success=int(time.time()); failures=0
                delivery.health(a.health or a.state+'.health.json',last_success=last_success,failures=0,pending=len(read_games(a.database,a.state,delivery.destination)),pending_is_lower_bound=True,error='',message='Registration healthy')
            except Exception as e:
                # Do not print HTTP bodies, URLs or payloads that might expose secrets.
                failures+=1
                code=getattr(e,'code',None)
                message='Check server ID/token' if code==403 else 'Check website database/configuration' if code==503 else 'Check URL, TLS certificate and connectivity'
                try: delivery.health(a.health or a.state+'.health.json',last_success=last_success,failures=failures,error=type(e).__name__,message=message)
                except OSError: pass
                print('Directory delivery deferred ('+type(e).__name__+'). '+message+'; gameplay continues.',file=sys.stderr)
            deadline=time.monotonic()+a.interval
            while not stop.is_set() and child.poll() is None and time.monotonic()<deadline:
                stop.wait(min(1,max(0,deadline-time.monotonic())))
    finally:
        if child.poll() is None:
            child.terminate()
            try: child.wait(timeout=5)
            except subprocess.TimeoutExpired: child.kill(); child.wait()
        delivery.db.close()
    return child.returncode or 0


if __name__ == '__main__':
    sys.exit(main())
