#!/usr/bin/env python3
"""Real sockets: ranked inputs, forged reports, hold, pause and elimination."""
import sqlite3,time,unittest
from test_http import server
from test_protocol import Peer

def start(port):
 p,q=Peer(port,'auth_winner'),Peer(port,'auth_loser')
 for peer in [p,q]:peer.send('clientinfo gui test 1.0.0 input-v1')
 p.join();q.join();p.send('playerready');q.send('playerready');p.sync();q.sync();p.send('startgame 1')
 p.until('newgame');q.until('newgame');p.until('statev1 ');q.until('statev1 ')
 return p,q

def snapshot(peer):
 peer.sync()
 peer.send('inputv1 0 left')
 return peer.until('statev1 ')[-1].split()

class AuthorityTests(unittest.TestCase):
 def test_forged_boards_clears_and_loss_do_not_control_ranked_game(self):
  with server(details=True) as (port,api,root):
   p,q=start(port)
   try:
    p.send('f 1 '+'1'*264);p.send('lineclear 4');p.send('holdpiece 6');p.send('holdclear');p.send('playerlost 1');p.sync()
    state=p.until('statev1 ')[-1].split()
    self.assertEqual(state[9:12],['0','0','1']);self.assertEqual(state[13],'0'*264)
    q.send('inputv1 1 left');first=q.until('statev1 ')[-1].split()
    q.send('inputv1 1 left');q.send('inputv1 2 hold');second=q.until('statev1 ')[-1].split()
    while second[1]!='2':second=q.until('statev1 ')[-1].split()
    held=second[7];q.send('inputv1 3 hold');third=q.until('statev1 ')[-1].split()
    while third[1]!='3':third=q.until('statev1 ')[-1].split()
    self.assertEqual(third[7],held);self.assertEqual(third[8],'1')
    for seq in range(4,100):
     q.send(f'inputv1 {seq} drop');time.sleep(.06)
     with sqlite3.connect(root/'db.sqlite3') as db:
      row=db.execute('SELECT ranked,input_protocol,ended_epoch,winner_nick FROM games').fetchone()
     if row[2]:break
    self.assertEqual(row[0:2],(1,'input-v1'));self.assertGreater(row[2],0);self.assertEqual(row[3],p.nick)
    with sqlite3.connect(root/'db.sqlite3') as db:
     self.assertGreater(db.execute('SELECT blocks_placed FROM game_stats WHERE nick=?',(q.nick,)).fetchone()[0],0)
   finally:p.close();q.close()
 def test_pause_and_room_transfer_preserve_input_contract(self):
  with server(details=True) as (port,api,root):
   p,q=start(port)
   try:
    p.send('pause 1');p.sync()
    q.send('inputv1 1 drop');q.sync()
    with sqlite3.connect(root/'db.sqlite3') as db:self.assertEqual(db.execute('SELECT blocks_placed FROM game_stats WHERE nick=?',(q.nick,)).fetchone()[0],0)
    p.send('pause 0');p.sync();q.send('inputv1 2 drop');q.sync()
    with sqlite3.connect(root/'db.sqlite3') as db:self.assertEqual(db.execute('SELECT blocks_placed FROM game_stats WHERE nick=?',(q.nick,)).fetchone()[0],4)
    q.send('leaveLobby');q.sync();q.join(2);q.send('playerready');q.sync();q.send('startgame 2');q.until('newgame');q.until('statev1 ')
   finally:p.close();q.close()
 def test_legacy_rounds_are_casual(self):
  with server(details=True) as (port,api,root):
   p,q=Peer(port,'legacy_one'),Peer(port,'legacy_two')
   try:
    p.join();q.join();p.send('playerready');q.send('playerready');p.sync();q.sync();p.send('startgame 1');p.until('newgame')
    with sqlite3.connect(root/'db.sqlite3') as db:self.assertEqual(db.execute('SELECT ranked,input_protocol FROM games').fetchone(),(0,'legacy'))
   finally:p.close();q.close()
if __name__=='__main__':unittest.main()
