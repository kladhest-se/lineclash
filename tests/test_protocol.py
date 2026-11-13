#!/usr/bin/env python3
"""Portable trust-boundary and reconnect integration regressions."""
import hashlib
import hmac
import socket
import sqlite3
import tempfile
from pathlib import Path
import time
import unittest
from test_http import server, request

KEY = hashlib.sha256(b'test-password').hexdigest()
class Peer:
    def __init__(self, port, nick):
        self.nick = nick
        self.sock = socket.create_connection(('127.0.0.1',port),2)
        self.sock.settimeout(3)
        self.stream = self.sock.makefile('rb')
        self.send('authnick '+nick)
        lines = self.until('authchallenge ')
        challenge = lines[-1].split()[1]
        response = KEY if challenge=='new' else hmac.new(bytes.fromhex(KEY),challenge.encode(),hashlib.sha256).hexdigest()
        self.send('authresp '+response)
        self.until('authok ')
    def send(self, text): self.sock.sendall((text+'\n').encode())
    def until(self, marker):
        lines=[]
        for _ in range(300):
            raw=self.stream.readline()
            if not raw: raise AssertionError('Connection closed waiting for '+marker)
            line=raw.decode().strip(); lines.append(line)
            if marker in line: return lines
        raise AssertionError('Missing '+marker)
    def join(self, room=1):
        self.send(f'lineclash_join {self.nick} - {room}')
        return int(self.until('playernum ')[-1].split()[1])
    def sync(self):
        self.send('ping barrier'); self.until('pong barrier')
    def close(self): self.stream.close(); self.sock.close()

class ProtocolTests(unittest.TestCase):
    def test_stopped_games_are_recorded_with_and_without_ranked_stats(self):
        for humans in (1,2):
            with self.subTest(humans=humans), server() as (port,api):
                peers=[Peer(port,'record'+str(i)) for i in range(humans)]
                try:
                    for p in peers: p.join(); p.send('playerready'); p.sync()
                    peers[0].send('startgame 1'); peers[0].until('newgame')
                    peers[0].send('endgame'); peers[0].until('endgame'); peers[0].sync()
                    games=request(api)['recent_games']
                    self.assertEqual(len(games),1)
                    self.assertTrue(games[0]['ended_at'])
                    self.assertEqual(games[0]['winner'],'')
                    self.assertEqual(games[0]['players'],humans)
                finally:
                    for p in peers: p.close()

    def test_ranked_result_epochs_participants_and_public_replay(self):
        with server(details=True) as (port,api,root):
            p,q=Peer(port,'winner'),Peer(port,'loser')
            try:
                p.join();q.join();p.send('playerready');q.send('playerready');p.sync();q.sync()
                p.send('startgame 1');p.until('newgame');q.until('newgame')
                p.send('gmsg PRIVATE_CHAT_NOT_IN_REPLAY');p.sync()
                q.send('playerlost 2');p.until('endgame');p.sync()
                with sqlite3.connect(root/'db.sqlite3') as db:
                    game=db.execute('SELECT game_id,ranked,started_epoch,ended_epoch,end_reason FROM games').fetchone()
                    self.assertEqual(game[1],0);self.assertGreater(game[2],0);self.assertGreaterEqual(game[3],game[2]);self.assertEqual(game[4],'completed')
                    players=db.execute('SELECT nick,placement,won,game_ended FROM game_stats ORDER BY nick').fetchall()
                    self.assertEqual([(x[0],x[1],x[2]) for x in players],[('loser',2,0),('winner',1,1)])
                    self.assertTrue(all(x[3] for x in players))
                    replay='\n'.join(x[0] for x in db.execute('SELECT line FROM replay_events ORDER BY seq'))
                    self.assertIn('newgame ',replay);self.assertIn('endgame',replay);self.assertNotIn('PRIVATE_CHAT',replay);self.assertNotIn('auth',replay)
            finally:p.close();q.close()

    def test_host_departure_and_simultaneous_disconnect(self):
        with server(settings='reconnect_window=0\n',details=True) as (port,api,root):
            peers=[Peer(port,'leave'+str(i)) for i in range(3)]
            try:
                for p in peers:p.join();p.send('playerready');p.sync()
                peers[0].send('startgame 1')
                for p in peers:p.until('newgame')
                peers[0].close();time.sleep(.1);peers[1].sync()
                peers[1].send('endgame');peers[1].until('endgame');peers[1].sync()
                with sqlite3.connect(root/'db.sqlite3') as db:
                    self.assertEqual(db.execute('SELECT ranked,end_reason FROM games').fetchone(),(0,'stopped'))
                    self.assertEqual(db.execute("SELECT COUNT(*) FROM game_stats WHERE game_ended='' ").fetchone()[0],0)
                peers[1].close();peers[2].close();time.sleep(.1)
                self.assertEqual(request(api)['players']['online'],0)
            finally:
                for p in peers:
                    try:p.close()
                    except OSError:pass

    def test_database_generation_survives_server_restart(self):
        with tempfile.TemporaryDirectory() as d:
            path=Path(d)/'persist.sqlite3';settings=f'db_file={path}\n'
            with server(settings=settings) as (port,api):
                with sqlite3.connect(path) as db: identity=db.execute("SELECT value FROM service_metadata WHERE key='database_id'").fetchone()[0]
            with server(settings=settings) as (port,api):
                with sqlite3.connect(path) as db:self.assertEqual(db.execute("SELECT value FROM service_metadata WHERE key='database_id'").fetchone()[0],identity)

    def test_join_cannot_impersonate_another_account(self):
        with server() as (port, api):
            p=Peer(port,'attacker')
            try:
                p.send('lineclash_join administrator - 1')
                self.assertIn('Nickname must match',p.until('joinerr ')[-1])
                self.assertEqual(request(api)['rooms'][0]['players'],0)
                self.assertEqual(p.join(),1)
            finally: p.close()

    def test_pause_field_validation_and_unearned_special(self):
        with server() as (port,api):
            p=Peer(port,'player'); watcher=None
            try:
                slot=p.join(); p.send('playerready'); p.send('startgame 1'); p.until('newgame')
                watcher=Peer(port,'watcher'); watcher.join()
                p.send(f'pline {slot} initial_barrier'); watcher.until('initial_barrier')
                # Unearned self-nukes used to synthesize inventory and be relayed.
                p.send(f'sb {slot} n {slot}')
                p.send(f'pline {slot} special_barrier')
                lines=watcher.until('special_barrier')
                self.assertFalse(any(x.startswith('sb ') for x in lines),lines)
                p.send(f'f {slot} bad')
                p.send(f'f {slot} '+('z'*264))
                p.send(f'pline {slot} invalid_barrier')
                self.assertFalse(any(x.startswith('f ') for x in watcher.until('invalid_barrier')))
                p.send('pause 1'); watcher.until('pause 1')
                p.send('lineclear 4'); p.send(f'f {slot} '+('1'*264))
                p.send(f'pline {slot} pause_barrier')
                self.assertFalse(any(x.startswith('f ') for x in watcher.until('pause_barrier')))
                self.assertEqual(request(api)['server_stats']['lines_cleared'],0)
                p.send('pause 0'); watcher.until('pause 0')
                p.send(f'f {slot} '+('1'*264))
                p.send(f'pline {slot} valid_barrier')
                self.assertTrue(any(x==f'f {slot} '+('1'*264) for x in watcher.until('valid_barrier')))
            finally:
                p.close()
                if watcher:watcher.close()

    def test_special_type_cannot_be_substituted_or_sender_spoofed(self):
        with server() as (port,_):
            p=Peer(port,'inventory'); q=None
            try:
                slot=p.join(); p.send('playerready'); p.send('startgame 1'); p.until('newgame')
                q=Peer(port,'observer'); q.join()
                p.send('lineclear 1'); sp=p.until('addspecial ')[-1].split()[1]
                wrong='a' if sp!='a' else 'n'
                p.send(f'sb {slot} {wrong} 8'); p.send(f'pline {slot} mismatch_barrier')
                self.assertFalse(any(x.startswith('sb ') for x in q.until('mismatch_barrier')))
                p.send(f'sb {slot} {sp} 8'); p.send(f'pline {slot} correct_barrier')
                lines=q.until('correct_barrier')
                self.assertIn(f'sb {slot} {sp} {slot}',lines)
                p.send(f'sb {slot} {sp} 8'); p.send(f'pline {slot} consumed_barrier')
                self.assertFalse(any(x.startswith('sb ') for x in q.until('consumed_barrier')))
            finally:
                p.close()
                if q:q.close()

    def test_resume_in_nonfull_room_preserves_slot_and_count(self):
        with server(settings='speedup = off\nreconnect_window = 5\n') as (port,api):
            p=Peer(port,'resumer'); q=Peer(port,'other')
            try:
                slot=p.join(); q.join(); p.send('playerready'); q.send('playerready'); q.sync()
                p.send('startgame 1'); p.until('newgame'); q.until('newgame')
                board='0'*252+'111111111110'
                p.send(f'f {slot} '+board); p.sync(); p.close()
                # Read the reservation notice rather than sleeping for the server.
                q.until('reconnect window')
                p=Peer(port,'resumer'); p.send('lineclash_join resumer - 1 resume 1')
                self.assertEqual(int(p.until('playernum ')[-1].split()[1]),slot)
                self.assertEqual(p.until('resume_ok ')[-1],'resume_ok 1 1')
                self.assertEqual(p.until('resume_field ')[-1],'resume_field '+board)
                p.send('listlobbies')
                info=p.until('lobbyinfo 1 ')[-1].split()
                self.assertEqual(int(info[2]),2)
                self.assertEqual(request(api)['rooms'][0]['playing'],2)
            finally: p.close(); q.close()

    def test_expiry_with_speedup_disabled_does_not_join_new_round(self):
        with server(settings='speedup = off\nreconnect_window = 1\n') as (port,_):
            p=Peer(port,'expires'); q=Peer(port,'stays')
            try:
                p.join(); q.join(); p.send('playerready'); q.send('playerready'); q.sync()
                p.send('startgame 1'); p.until('newgame'); q.until('newgame'); p.close()
                q.until('reconnect window expired')
                p=Peer(port,'expires'); p.send('lineclash_join expires - 1 resume 1')
                self.assertIn('reconnect_failed',p.until('reconnect_failed')[-1])
                p.send('listlobbies'); info=p.until('lobbyinfo 1 ')[-1].split()
                self.assertEqual(int(info[2]),1)
            finally:p.close(); q.close()

if __name__=='__main__':unittest.main()
