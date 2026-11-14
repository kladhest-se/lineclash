#!/usr/bin/env python3
"""A daemon with /dev/null stdin must stay responsive without spinning on EOF."""
from pathlib import Path
import resource
import socket
import subprocess
import tempfile
import time

ROOT=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as directory:
    base=Path(directory)
    with socket.socket() as listener:listener.bind(('127.0.0.1',0));port=listener.getsockname()[1]
    cfg=base/'server.cfg'
    cfg.write_text(f'port = {port}\ndb_file = {base}/database.sqlite3\nlog_file = {base}/server.log\npublic_server = off\nlan_discovery = off\napi_enabled = off\n')
    before=resource.getrusage(resource.RUSAGE_CHILDREN);started=time.monotonic()
    process=subprocess.Popen([str(ROOT/'server/lineclashd'),'--config',str(cfg)],stdin=subprocess.DEVNULL,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    try:
        deadline=started+5
        while True:
            try:
                with socket.create_connection(('127.0.0.1',port),timeout=.1):break
            except OSError:
                if time.monotonic()>deadline:raise AssertionError('daemon did not start')
                time.sleep(.02)
        time.sleep(.8)
        assert process.poll() is None,'daemon exited on stdin EOF'
        with socket.create_connection(('127.0.0.1',port),timeout=.2):pass
    finally:
        process.terminate();process.wait(timeout=5)
    after=resource.getrusage(resource.RUSAGE_CHILDREN)
    cpu=after.ru_utime+after.ru_stime-before.ru_utime-before.ru_stime
    elapsed=time.monotonic()-started
    assert cpu<elapsed*.5,f'daemon busy-looped after stdin EOF: {cpu:.3f}s CPU / {elapsed:.3f}s elapsed'
    print(f'Closed-stdin daemon stayed responsive: {cpu:.3f}s CPU / {elapsed:.3f}s elapsed')
