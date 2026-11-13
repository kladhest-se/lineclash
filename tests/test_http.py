#!/usr/bin/env python3
"""Real HTTP regressions using only the Python standard library."""
import contextlib
import json
import pathlib
import socket
import subprocess
import sys
import tempfile
import time
import unittest

BINARY = pathlib.Path(sys.argv.pop(1) if len(sys.argv) > 1 else '../server/lineclashd').resolve()


def free_port():
    with socket.socket() as s:
        s.bind(('127.0.0.1', 0))
        return s.getsockname()[1]


@contextlib.contextmanager
def server(enabled=True, settings="", details=False):
    with tempfile.TemporaryDirectory(prefix='lineclash-http-') as d:
        root = pathlib.Path(d)
        port, api = free_port(), free_port()
        while api == port:
            api = free_port()
        cfg = root / 'server.cfg'
        cfg.write_text(f'port = {port}\nstatus_port = {api}\n'
                       f'status_api = {"on" if enabled else "off"}\n'
                       f'db_file = {root}/db.sqlite3\nlog_file = {root}/server.log\n'
                       'server_name = Frössman Quote" and slash\\ and\ttab\n' + settings)
        with (root / 'startup.log').open('w+') as log:
            p = subprocess.Popen([str(BINARY), '--config', str(cfg)], stdin=subprocess.PIPE,
                                 stdout=log, stderr=log)
            try:
                deadline = time.monotonic() + 4
                while True:
                    if p.poll() is not None or time.monotonic() > deadline:
                        log.seek(0)
                        raise AssertionError('Server failed to start:\n' + log.read())
                    try:
                        with socket.create_connection(('127.0.0.1', port), .1):
                            break
                    except OSError:
                        time.sleep(.02)
                yield (port, api, root) if details else (port, api)
            finally:
                p.terminate()
                try:
                    p.wait(timeout=2)
                except subprocess.TimeoutExpired:
                    p.kill()
                    p.wait()
                p.stdin.close()


def request(port):
    with socket.create_connection(('127.0.0.1', port), 2) as s:
        s.sendall(b'GET /status HTTP/1.0\r\nHost: localhost\r\n\r\n')
        chunks = []
        while True:
            data = s.recv(65536)
            if not data:
                break
            chunks.append(data)
        headers, body = b''.join(chunks).split(b'\r\n\r\n', 1)
        length = int(next(line.split(b':', 1)[1] for line in headers.split(b'\r\n')
                          if line.lower().startswith(b'content-length:')))
        assert len(body) == length
        return json.loads(body)


class HttpTests(unittest.TestCase):
    def test_json_and_incomplete_clients(self):
        with server() as (game, api):
            doc = request(api)
            self.assertEqual(doc['server_name'], 'Frössman Quote" and slash\\ and\ttab')
            self.assertNotEqual(doc['version'], 'dev')
            self.assertIn('server_stats', doc)
            # Fragmented/incomplete requests must not stall HTTP or game traffic.
            with socket.create_connection(('127.0.0.1', api), 2) as slow:
                slow.sendall(b'GET / HTTP/1.0\r\n')
                self.assertIn('rooms', request(api))
                with socket.create_connection(('127.0.0.1', game), 2) as player:
                    player.sendall(b'authnick httptest\n')
                    data = b''
                    while b'authchallenge new' not in data:
                        data += player.recv(4096)
                slow.sendall(b'\r\n')
                response = b''
                while True:
                    chunk = slow.recv(65536)
                    if not chunk:
                        break
                    response += chunk
                json.loads(response.split(b'\r\n\r\n', 1)[1])

    def test_incomplete_request_expires(self):
        with server() as (_, api):
            with socket.create_connection(('127.0.0.1', api), 2) as s:
                s.settimeout(7)
                s.sendall(b'GET / HTTP/1.0\r\n')
                self.assertEqual(s.recv(1), b'')
            self.assertIn('rooms', request(api))

    def test_disabled_api(self):
        with server(False) as (_, api):
            with self.assertRaises(OSError):
                socket.create_connection(('127.0.0.1', api), .2)


if __name__ == '__main__':
    unittest.main()
