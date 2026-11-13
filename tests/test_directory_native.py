#!/usr/bin/env python3
"""Real native game-port HMAC proof and UDP LAN response; no public announcements."""
import hashlib,hmac,json,os,shutil
from pathlib import Path
import socket,subprocess,sys,tempfile,time
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as folder:
 folder=Path(folder);key=os.urandom(32).hex();db=folder/'game.sqlite3'
 Path(str(db)+'.directory-key').write_text(key)
 with socket.socket() as sock:sock.bind(('127.0.0.1',0));port=sock.getsockname()[1]
 cfg=folder/'server.cfg';cfg.write_text(f'port = {port}\ndb_file = {db}\nlog_file = {folder}/server.log\nstatus_api = off\npublic_server = on\nlan_discovery = on\n')
 env=dict(os.environ,LINECLASH_DIRECTORY_DISABLE='1')
 with (folder/'output.log').open('w') as log:
  p=subprocess.Popen([str(root/('server/lineclashd.exe' if os.name=='nt' else 'server/lineclashd')),'--config',str(cfg)],env=env,stdout=log,stderr=log)
  try:
   for _ in range(100):
    try:connection=socket.create_connection(('127.0.0.1',port),.2);break
    except OSError:time.sleep(.03)
   else:raise AssertionError((folder/'output.log').read_text())
   with connection:
    connection.settimeout(2);nonce=os.urandom(32).hex();connection.sendall(f'directory-proof {nonce}\n'.encode());data=b''
    expected=hmac.new(bytes.fromhex(key),('lineclash-directory-v1:'+nonce).encode(),hashlib.sha256).hexdigest().encode()
    while b'directory-proof '+expected not in data:data+=connection.recv(4096)
   php="require "+json.dumps(str(root/'public-web/inc/service.php'))+f"; if(!directory_verify_endpoint('127.0.0.1',{port},'{key}'))exit(1); if(directory_verify_endpoint('127.0.0.1',{port},'"+'f'*64+"'))exit(2);"
   if shutil.which('php'):subprocess.run(['php','-r',php],check=True,timeout=8)
   with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as udp:
    udp.settimeout(2);udp.sendto(b'LC-LAN-1 0123456789abcdef',('127.0.0.1',31459));response,_=udp.recvfrom(1024)
    assert response.startswith(f'LC-LAN-1\t0123456789abcdef\t{port}\t'.encode()),response
   discovery=root/'tests/test_discovery'
   if discovery.exists():subprocess.run([str(discovery),'--lan',str(port)],check=True,timeout=5)
   print('PASS: native endpoint proof, PHP verification, wrong-key rejection and nonce-matched LAN discovery')
  finally:p.terminate();p.wait(timeout=12)
