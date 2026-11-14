#!/usr/bin/env python3
import importlib.util
from pathlib import Path
import hashlib
import json
import sqlite3
import tempfile
import unittest

spec=importlib.util.spec_from_file_location('directory',Path(__file__).resolve().parents[1]/'scripts/server_directory.py')
d=importlib.util.module_from_spec(spec); spec.loader.exec_module(d)
vs=importlib.util.spec_from_file_location('verify_replay',Path(__file__).resolve().parents[1]/'scripts/verify_replay.py');v=importlib.util.module_from_spec(vs);vs.loader.exec_module(v)

class DirectoryTests(unittest.TestCase):
    def test_downloaded_replay_verification(self):
        events=[dict(elapsed_ms=0,line='f 1 '+'0'*264),dict(elapsed_ms=100,line='endgame')]
        raw=json.dumps(events,separators=(',',':'));document=dict(match=dict(replay_sha256=hashlib.sha256(raw.encode()).hexdigest(),replay_truncated=0),replay_source=raw,events=events)
        result=v.verify(document);self.assertFalse(result['partial']);self.assertEqual(result['snapshots'],1)
        document['replay_source']=raw+' '
        with self.assertRaises(ValueError):v.verify(document)

    def test_url_policy(self):
        for url in ['http://example.org/api/', 'https://user:pass@example.org/', 'https://example.org/#token', 'file:///tmp/a']:
            with self.assertRaises(ValueError): d.endpoint(url)
        self.assertEqual(d.endpoint('http://127.0.0.1:123/api/',True),'http://127.0.0.1:123/api/')
        with self.assertRaises(ValueError): d.endpoint('http://example.org/',True)
        self.assertEqual(d.endpoint('https://example.org/api/'),'https://example.org/api/')

    def test_durable_ack_and_out_of_order(self):
        with tempfile.TemporaryDirectory() as folder:
            state=Path(folder)/'delivery.sqlite3'
            delivery=d.Delivery(state,'https://one/api/','one')
            games=[{'game_id':2}]
            self.assertEqual(delivery.batch(games),games)
            with self.assertRaises(ValueError): delivery.ack(games,{'ok':True,'ack':[1]})
            self.assertEqual(delivery.batch(games),games)
            delivery.ack(games,{'ok':True,'ack':[2]})
            identity=delivery.identity; delivery.db.close()
            delivery=d.Delivery(state,'https://one/api/','one')
            self.assertEqual(identity,delivery.identity)
            self.assertEqual(delivery.batch([{'game_id':1},{'game_id':2}]),[{'game_id':1}])
            delivery.db.close()
            delivery=d.Delivery(state,'https://two/api/','one')
            self.assertEqual(delivery.batch(games),games); delivery.db.close()

    def test_generation_survives_delivery_reset_and_changes_on_database_reset(self):
        with tempfile.TemporaryDirectory() as folder:
            source=Path(folder)/'game.sqlite3';state=Path(folder)/'state.sqlite3'
            delivery=d.Delivery(state,'https://example.org/api/','main');delivery.bind(source)
            identity=delivery.identity;destination=delivery.destination
            delivery.ack([{'game_id':1}],{'ok':True,'ack':[1]});delivery.db.close()
            state.unlink();delivery=d.Delivery(state,'https://example.org/api/','main');delivery.bind(source)
            self.assertEqual(delivery.identity,identity);self.assertEqual(delivery.destination,destination)
            source.unlink();delivery.bind(source)
            self.assertNotEqual(delivery.identity,identity);self.assertNotEqual(delivery.destination,destination)
            delivery.db.close()

    def test_central_identity_metadata_is_reported(self):
        with tempfile.TemporaryDirectory() as folder:
            path=Path(folder)/'game.sqlite3'
            with sqlite3.connect(path) as db:
                db.execute('CREATE TABLE games(game_id,room_id,started_at,ended_at,winner_nick,winner_team,player_count)')
                db.execute("INSERT INTO games VALUES(1,1,'start','end','@alice','Red',2)")
                db.execute('CREATE TABLE game_stats(stat_id,game_id,nick,lines_cleared,specials_sent,blocks_placed,placement,won)')
                db.execute("INSERT INTO game_stats VALUES(1,1,'@alice',10,2,30,1,1)")
                db.execute('CREATE TABLE central_identities(account_id,nick)')
                db.execute('INSERT INTO central_identities VALUES(?,?)',('a'*32,'@alice'))
            game=d.read_games(path)[0]
            self.assertEqual(game['winner_account_id'],'a'*32)
            self.assertEqual(game['players'][0]['account_id'],'a'*32)

    def test_only_completed_public_fields(self):
        with tempfile.TemporaryDirectory() as folder:
            path=Path(folder)/'game.sqlite3'
            with sqlite3.connect(path) as db:
                db.execute('CREATE TABLE games(game_id,room_id,started_at,ended_at,winner_nick,winner_team,player_count,secret)')
                db.execute("INSERT INTO games VALUES(1,1,'start','','','','2','private')")
                db.execute("INSERT INTO games VALUES(2,2,'start','end','winner','team',2,'private')")
            games=d.read_games(path)
            self.assertEqual([g['game_id'] for g in games],[2])
            self.assertNotIn('secret',games[0])
            state=Path(folder)/'state.sqlite3'
            delivery=d.Delivery(state,'https://example.org/api/','test')
            self.assertEqual(d.read_games(path,state,delivery.destination),games)
            delivery.ack(games,{'ok':True,'ack':[2]})
            self.assertEqual(d.read_games(path,state,delivery.destination),[])
            with sqlite3.connect(path) as db: db.execute("UPDATE games SET ended_at='finished later' WHERE game_id=1")
            self.assertEqual([g['game_id'] for g in d.read_games(path,state,delivery.destination)],[1])
            delivery.db.close()

if __name__=='__main__': unittest.main()
