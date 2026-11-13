#!/usr/bin/env python3
"""Exercise the real terminal client against a controllable local peer."""
import hashlib
import hmac
import os
import pathlib
import socket
import subprocess
import sys
import tempfile
import unittest
if os.name!='nt':
    import pty
BINARY=pathlib.Path(sys.argv.pop(1) if len(sys.argv)>1 else '../clients/cli/lineclash-cli').resolve()

class Wire:
    def __init__(self,listener):
        self.s,_=listener.accept(); self.s.settimeout(3); self.f=self.s.makefile('rb')
    def send(self,line):self.s.sendall((line+'\n').encode())
    def until(self,needle):
        for _ in range(300):
            data=self.f.readline()
            if not data:raise AssertionError('client closed waiting for '+needle)
            line=data.decode().strip()
            if needle in line:return line
        raise AssertionError('missing '+needle)
    def auth(self):
        self.send('noconnecting LineClash/1.0'); self.until('authnick tester')
        nonce='b'*64; self.send('authchallenge '+nonce)
        expected=hmac.new(hashlib.sha256(b'password').digest(),nonce.encode(),hashlib.sha256).hexdigest()
        assert self.until('authresp ')==('authresp '+expected)
        self.send('authok existing'); self.until('reqleaderboard')
        self.send('reconnectwindow 3')
    def close(self):self.f.close(); self.s.close()

@unittest.skipIf(os.name=='nt','Terminal client is POSIX-only')
class CliReconnectTests(unittest.TestCase):
    def run_case(self,kick=False):
        with tempfile.TemporaryDirectory(prefix='lineclash-cli-') as d, socket.socket() as listener:
            listener.bind(('127.0.0.1',0)); listener.listen(); listener.settimeout(3)
            master,slave=pty.openpty()
            with open(pathlib.Path(d)/'client.log','w+') as log:
                p=subprocess.Popen([str(BINARY),'--cfg',str(pathlib.Path(d)/'profiles'),
                                    '127.0.0.1',str(listener.getsockname()[1]),'tester','password'],
                                   stdin=slave,stdout=log,stderr=log)
                os.close(slave); peers=[]
                try:
                    first=Wire(listener); peers.append(first); first.auth()
                    if kick:
                        first.send('kicked Administrative kick'); first.send('noconnecting Kicked')
                        first.close(); peers.remove(first); p.wait(timeout=3)
                        listener.settimeout(.5)
                        with self.assertRaises(socket.timeout):listener.accept()
                    else:
                        first.send('lobbyinfo 1 0 8 open 0 1 0 0 1 0 0 1 Room')
                        os.write(master,b'\r')
                        first.until('lineclash_join tester - 1')
                        first.send('playernum 1'); first.send('roundid 7'); first.send('newgame')
                        # A field update proves the client entered play before the cut.
                        os.write(master,b'\x1b[C'); first.until('f 1 ')
                        first.close(); peers.remove(first)
                        second=Wire(listener); peers.append(second); second.auth()
                        self.assertEqual(second.until('lineclash_join '),'lineclash_join tester - 1 resume 7')
                        second.send('playernum 1'); second.send('resume_ok 7 1'); second.send('ingame')
                        os.write(master,b'\x1b[C'); second.until('f 1 ')
                        second.close(); peers.remove(second)
                        os.write(master,b'q'); p.wait(timeout=3)
                    self.assertEqual(p.returncode,0)
                finally:
                    for peer in peers:peer.close()
                    if p.poll() is None:p.terminate(); p.wait(timeout=3)
                    os.close(master)
    def test_drop_reauthenticates_and_resumes_without_input(self):self.run_case()
    def test_kick_does_not_retry(self):self.run_case(True)
if __name__=='__main__':unittest.main()
