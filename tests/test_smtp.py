#!/usr/bin/env python3
"""SMTP wire tests with local relays only; no real email is sent."""
import base64,json,os,shutil,socket,ssl,subprocess,tempfile,threading,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]

class Relay:
 def __init__(self,mode='none',context=None,advertise_tls=True,auth='PLAIN',reject=False,drop_quit=False):
  self.mode=mode;self.context=context;self.advertise_tls=advertise_tls;self.auth=auth;self.reject=reject;self.drop_quit=drop_quit
  self.commands=[];self.messages=[];self.error=None
  self.socket=socket.socket();self.socket.bind(('127.0.0.1',0));self.socket.listen();self.port=self.socket.getsockname()[1];self.socket.settimeout(8)
  self.thread=threading.Thread(target=self.run,daemon=True);self.thread.start()
 def run(self):
  try:
   with self.socket.accept()[0] as initial:
    conn=initial;conn.settimeout(8)
    if self.mode=='tls':conn=self.context.wrap_socket(conn,server_side=True)
    f=conn.makefile('rwb',buffering=0)
    def reply(s):f.write((s+'\r\n').encode())
    reply('220 localhost fixture')
    auth_step=0
    while True:
     line=f.readline().decode().rstrip('\r\n')
     if not line:break
     self.commands.append(line)
     if auth_step:
      auth_step-=1;reply('334 UGFzc3dvcmQ6' if auth_step else '235 Authenticated');continue
     if line.startswith('EHLO '):
      reply('250-localhost')
      if self.advertise_tls:reply('250-STARTTLS')
      reply('250 AUTH '+self.auth)
     elif line=='STARTTLS':
      reply('220 Start TLS');f.close();conn=self.context.wrap_socket(conn,server_side=True);f=conn.makefile('rwb',buffering=0)
     elif line=='AUTH LOGIN':auth_step=2;reply('334 VXNlcm5hbWU6')
     elif line.startswith('AUTH PLAIN '):reply('235 Authenticated')
     elif line.startswith('MAIL FROM:'):reply('250 Sender accepted')
     elif line.startswith('RCPT TO:'):reply('550 Rejected' if self.reject else '250 Recipient accepted')
     elif line=='DATA':
      reply('354 Send data');message=b''
      while True:
       raw=f.readline()
       if raw in (b'.\r\n',b''):break
       message+=raw
      self.messages.append(message);reply('250 Queued')
     elif line=='QUIT':
      if not self.drop_quit:reply('221 Bye')
      break
     else:raise AssertionError('Unexpected SMTP command')
    f.close();conn.close()
  except (ssl.SSLError,ConnectionError,BrokenPipeError):pass # Expected TLS rejection tests.
  except Exception as e:self.error=e
  finally:self.socket.close()
 def close(self):
  self.thread.join(10)
  assert not self.thread.is_alive(),'Relay did not stop'
  if self.error:raise self.error

@unittest.skipUnless(shutil.which('php') and shutil.which('openssl'),'PHP and OpenSSL CLI required')
class SMTPTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.temp=tempfile.TemporaryDirectory();cls.path=Path(cls.temp.name)
  (cls.path/'cert.cnf').write_text('[req]\ndistinguished_name=dn\nx509_extensions=ext\nprompt=no\n[dn]\nCN=localhost\n[ext]\nsubjectAltName=DNS:localhost,IP:127.0.0.1\nbasicConstraints=critical,CA:TRUE\nkeyUsage=critical,digitalSignature,keyEncipherment,keyCertSign\n')
  subprocess.run(['openssl','req','-x509','-newkey','rsa:2048','-nodes','-days','1','-keyout',str(cls.path/'key.pem'),'-out',str(cls.path/'cert.pem'),'-config',str(cls.path/'cert.cnf')],check=True,capture_output=True)
  cls.context=ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER);cls.context.load_cert_chain(cls.path/'cert.pem',cls.path/'key.pem')
 @classmethod
 def tearDownClass(cls):cls.temp.cleanup()
 def send(self,relay,**overrides):
  settings=dict(host='127.0.0.1',port=relay.port,encryption=relay.mode,from_address='noreply@example.test',hello_host='localhost',**overrides)
  code='require $argv[1];$c=json_decode(stream_get_contents(STDIN),true);$s=smtp_settings(["smtp"=>$c]);if(!$s){echo "disabled";exit;}try{smtp_send($s,"recipient@example.test","Fixture", "Body\\n.with dot");echo "sent";}catch(Throwable $e){echo $e->getMessage();}'
  p=subprocess.run(['php','-r',code,str(ROOT/'public-web/inc/smtp.php')],input=json.dumps(settings),text=True,capture_output=True,timeout=12)
  self.assertEqual(p.returncode,0,p.stderr);relay.close();return p.stdout
 def test_plain_no_auth_and_encoded_body(self):
  relay=Relay();self.assertEqual(self.send(relay),'sent');self.assertFalse(any(c.startswith('AUTH') for c in relay.commands))
  message=relay.messages[0];self.assertIn(b'From:',message);self.assertEqual(base64.b64decode(message.split(b'\r\n\r\n')[1]),b'Body\n.with dot')
 def test_plain_auth(self):
  relay=Relay();self.assertEqual(self.send(relay,username='fixture',password='secret'),'sent');self.assertTrue(any(c.startswith('AUTH PLAIN ') for c in relay.commands))
 def test_login_auth(self):
  relay=Relay(auth='LOGIN');self.assertEqual(self.send(relay,username='fixture',password='secret'),'sent');self.assertIn('AUTH LOGIN',relay.commands)
 def test_starttls(self):
  relay=Relay(mode='starttls',context=self.context);self.assertEqual(self.send(relay,ca_file=str(self.path/'cert.pem'),username='fixture',password='secret'),'sent')
  self.assertEqual(sum(c.startswith('EHLO ') for c in relay.commands),2);self.assertLess(relay.commands.index('STARTTLS'),next(i for i,c in enumerate(relay.commands) if c.startswith('AUTH ')))
 def test_source_interface(self):
  relay=Relay();self.assertEqual(self.send(relay,source_ip='127.0.0.1'),'sent')
 def test_hostname_verification(self):
  (self.path/'other.cnf').write_text((self.path/'cert.cnf').read_text().replace('DNS:localhost,IP:127.0.0.1','DNS:other.example.test'))
  subprocess.run(['openssl','req','-x509','-newkey','rsa:2048','-nodes','-days','1','-keyout',str(self.path/'other.key'),'-out',str(self.path/'other.pem'),'-config',str(self.path/'other.cnf')],check=True,capture_output=True)
  context=ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER);context.load_cert_chain(self.path/'other.pem',self.path/'other.key')
  relay=Relay(mode='tls',context=context);self.assertIn('TLS verification failed',self.send(relay,ca_file=str(self.path/'other.pem')));self.assertFalse(relay.messages)
 def test_cli_does_not_need_database(self):
  relay=Relay()
  config=self.path/'cli.php'
  settings=dict(smtp=dict(host='127.0.0.1',port=relay.port,encryption='none',from_address='sender@example.test'),dsn='invalid',user='',password='')
  subprocess.run(['php','-r','file_put_contents($argv[1],"<?php return ".var_export(json_decode(stream_get_contents(STDIN),true),true).";");',str(config)],input=json.dumps(settings),text=True,check=True)
  result=subprocess.run(['php',str(ROOT/'public-web/bin/manage.php'),'smtp-test','recipient@example.test'],env=dict(os.environ,LINECLASH_WEB_CONFIG=str(config)),capture_output=True,text=True,timeout=12)
  relay.close();self.assertEqual(result.returncode,0,result.stderr);self.assertIn('accepted',result.stdout);self.assertEqual(len(relay.messages),1)
 def test_implicit_tls(self):
  relay=Relay(mode='tls',context=self.context);self.assertEqual(self.send(relay,ca_file=str(self.path/'cert.pem')),'sent')
 def test_untrusted_certificate(self):
  relay=Relay(mode='tls',context=self.context);self.assertIn('TLS verification failed',self.send(relay));self.assertEqual(relay.messages,[])
 def test_starttls_untrusted_certificate(self):
  relay=Relay(mode='starttls',context=self.context);self.assertIn('verification failed',self.send(relay,username='fixture',password='secret'));self.assertFalse(any(c.startswith('AUTH') for c in relay.commands))
 def test_starttls_no_downgrade(self):
  relay=Relay(mode='starttls',advertise_tls=False);self.assertIn('does not offer STARTTLS',self.send(relay,username='fixture',password='secret'));self.assertEqual(len(relay.commands),1)
 def test_recipient_rejection(self):
  relay=Relay(reject=True);self.assertIn('550',self.send(relay));self.assertFalse(relay.messages)
 def test_quit_failure_after_accepted_delivery(self):
  relay=Relay(drop_quit=True);self.assertEqual(self.send(relay),'sent');self.assertEqual(len(relay.messages),1)
 def test_config_validation(self):
  base=dict(host='localhost',from_address='sender@example.test')
  for changes in ({'host':'bad\r\nhost'},{'port':0},{'encryption':'ssl-maybe'},{'from_address':'sender@example.test\r\nBcc:other@example.test'},{'from_name':'Bad\nName'},{'source_ip':'host.example'},{'hello_host':'bad\nhelo'}):
   code='require $argv[1];echo smtp_settings(["smtp"=>json_decode(stream_get_contents(STDIN),true)])===null?"disabled":"enabled";'
   p=subprocess.run(['php','-r',code,str(ROOT/'public-web/inc/smtp.php')],input=json.dumps(dict(base,**changes)),text=True,capture_output=True,check=True)
   self.assertEqual(p.stdout,'disabled')
if __name__=='__main__':unittest.main()
