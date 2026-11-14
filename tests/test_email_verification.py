#!/usr/bin/env python3
"""Activation and optional password policy using isolated MariaDB and SMTP fixtures."""
import base64,hashlib,http.cookies,json,os,re,secrets,socket,subprocess,tempfile,time,urllib.error,urllib.parse,urllib.request
from pathlib import Path
from test_smtp import Relay
ROOT=Path(__file__).resolve().parents[1]
base=os.environ.get('LINECLASH_WEB_RESET_TEST_CONFIG')
if not base:
 print('SKIP: set LINECLASH_WEB_RESET_TEST_CONFIG to a test MariaDB config with CREATE DATABASE permission')
 raise SystemExit(0)
name='lineclash_activation_test_'+secrets.token_hex(8)
with tempfile.TemporaryDirectory(prefix='lineclash-activation-') as folder, tempfile.TemporaryFile() as log:
 config=Path(folder)/'web.php';env=dict(os.environ,LINECLASH_WEB_CONFIG=base)
 def php(code,*args,environment=env):
  p=subprocess.run(['php','-r','require $argv[1]; '+code,str(ROOT/'public-web/inc/service.php'),*args],env=environment,text=True,capture_output=True)
  assert p.returncode==0,p.stderr
  return p.stdout
 php('database()->exec("CREATE DATABASE `".$argv[2]."` CHARACTER SET utf8mb4");',name)
 try:
  php('$c=require web_config_path();$c["dsn"]=preg_replace("/dbname=[^;]+/","dbname=".$argv[2],$c["dsn"]);$c["trusted_proxies"]=["127.0.0.1"];file_put_contents($argv[3],"<?php return ".var_export($c,true).";");',name,str(config))
  local=dict(os.environ,LINECLASH_WEB_CONFIG=str(config))
  subprocess.run(['php',str(ROOT/'public-web/bin/manage.php'),'migrate'],env=local,check=True,capture_output=True)
  def sql(s):return php('database()->exec($argv[2]);',s,environment=local)
  def rows(s):return json.loads(php('echo json_encode(database()->query($argv[2])->fetchAll());',s,environment=local))
  def configure(required=False,relay=None):
   smtp=dict(host='127.0.0.1',port=relay.port,encryption='none',from_address='sender@example.test') if relay else None
   php('$c=require web_config_path();$c["account_require_email_verification"]=$argv[2]==="true";$c["smtp"]=json_decode($argv[3],true);$c["account_password_min_length"]=6;$c["public_url"]="https://fixed.example.test";file_put_contents(web_config_path(),"<?php return ".var_export($c,true).";");',json.dumps(required),json.dumps(smtp),environment=local)
  cookie='';csrf='';source=10;password='secret' # Six characters, enabled by the site policy.
  with socket.socket() as s:s.bind(('127.0.0.1',0));port=s.getsockname()[1]
  app=subprocess.Popen(['php','-d','opcache.enable=0','-S',f'127.0.0.1:{port}','-t',str(ROOT/'public-web'),str(ROOT/'public-web/dev-router.php')],env=local,stdout=log,stderr=log)
  def call(path='/account/verify/',data=None,api=False):
   global cookie,csrf
   headers={'Cookie':cookie,'X-Forwarded-For':f'198.18.33.{source}'};body=None
   if data is not None:
    headers['Content-Type']='application/json' if api else 'application/x-www-form-urlencoded'
    body=(json.dumps(data) if api else urllib.parse.urlencode(dict(csrf=csrf,**data))).encode()
   try:r=urllib.request.urlopen(urllib.request.Request(f'http://127.0.0.1:{port}'+path,body,headers),timeout=5)
   except urllib.error.HTTPError as e:r=e
   for value in r.headers.get_all('Set-Cookie',[]):
    c=http.cookies.SimpleCookie(value)
    if 'lineclash_account' in c:cookie='lineclash_account='+c['lineclash_account'].value
   text=r.read().decode();m=re.search(r'name="csrf" value="([a-f0-9]+)"',text)
   if m:csrf=m[1]
   return r.code,text,r.headers
  def api(action,**fields):return call('/api/',dict(action=action,**fields),True)[:2]
  def register(handle):return api('account-client-register',format='session',handle=handle,email=handle+'@example.test',password=password)
  def token(relay):
   relay.close();assert len(relay.messages)==1
   message=base64.b64decode(relay.messages[0].split(b'\r\n\r\n')[1]).decode()
   return re.search(r'https://fixed.example.test/account/verify/#([a-f0-9]{64})',message)[1]
  try:
   configure()
   for _ in range(100):
    try:call();break
    except OSError:time.sleep(.03)
   assert 'not required' in call()[1]
   status,body=register('legacy');assert status==200 and body.startswith('central-session '),body
   configure(True)
   status,body=register('nomail');assert status==400 and 'not configured' in body
   assert not rows("SELECT id FROM accounts WHERE handle='nomail'")
   assert api('account-client-login',login='legacy',password=password)[0]==200 # Existing accounts retain access.
   source+=1;relay=Relay();configure(True,relay)
   status,body=register('pending');assert status==200 and body.startswith('central-verification-required '),body
   first=token(relay);account=rows("SELECT * FROM accounts WHERE handle='pending'")[0]
   assert account['email_pending']==1 and account['active']==1
   record=rows('SELECT * FROM account_email_verifications')[0]
   assert record['token_hash']==hashlib.sha256(first.encode()).hexdigest() and first not in json.dumps(record)
   assert 86300<int(record['expires'])-time.time()<=86400
   assert not rows("SELECT * FROM account_sessions WHERE account_id='"+account['id']+"'")
   assert api('account-client-login',login='pending',password='wrong')[0]==401
   assert api('account-client-login',login='pending',password=password)[0]==403
   call('/account/');page=call('/account/',dict(action='login',email='pending@example.test',password=password))[1]
   assert 'Verify your email' in page and '<h1>@pending' not in page
   status,pin_reply=api('account-client-pin-begin');device,pin=pin_reply.split()[1:3]
   call('/account/pin/');call('/account/pin/',dict(action='lookup',pin=pin))
   page=call('/account/pin/',dict(action='login',pin=pin,login='pending',password=password))[1]
   assert 'Verify your email' in page and '<button name="action" value="approve">' not in page
   # Turning verification off allows a pending account temporarily, including PIN/browser login.
   configure(False)
   status,body=api('account-client-login',login='pending',password=password);assert status==200
   temporary_session=body.split()[1]
   call('/account/');assert '<h1>@pending' in call('/account/',dict(action='login',email='pending@example.test',password=password))[1]
   call('/account/pin/');assert 'Client approved' in call('/account/pin/',dict(action='approve',pin=pin))[1]
   configure(True)
   assert '<h1>@pending' not in call('/account/')[1]
   assert api('account-client-logout',session=temporary_session)[0]==401
   sql('UPDATE account_client_pins SET last_poll=0')
   assert api('account-client-pin-poll',device=device)[1].startswith('central-error ')
   # Legacy server-code redemption is gated too.
   key=secrets.token_hex(32);nonce=secrets.token_hex(32)
   sql("INSERT INTO directory_servers(id,name,host,port,token_hash,last_seen,status_json) VALUES('fixture','Fixture','127.0.0.1',31457,'"+hashlib.sha256(key.encode()).hexdigest()+"',"+str(int(time.time()))+",'{}')")
   sql("INSERT INTO account_devices(request_hash,code,server_id,account_id,expires,state) VALUES('"+hashlib.sha256(nonce.encode()).hexdigest()+"','ABCDEF12','fixture','"+account['id']+"',"+str(int(time.time())+300)+",'approved')")
   assert api('account-poll',server_key=key,nonce=nonce)[1]=='central-error Account unavailable'
   status,page,headers=call();assert headers['Cache-Control']=='no-store' and headers['Referrer-Policy']=='no-referrer' and first not in page
   csrf='0'*64;assert 'This form expired' in call(data=dict(action='confirm',token=first))[1]
   assert rows("SELECT email_pending FROM accounts WHERE handle='pending'")[0]['email_pending']==1
   sql('UPDATE account_email_verifications SET expires=1')
   assert 'invalid or expired' in call(data=dict(action='confirm',token=first))[1]
   source+=1;relay=Relay();configure(True,relay);call()
   notice=call(data=dict(action='resend',email='pending@example.test'))[1];assert 'If an account is waiting' in notice
   second=token(relay);assert second!=first
   assert 'invalid or expired' in call(data=dict(action='confirm',token=first))[1]
   assert 'Email verified.' in call(data=dict(action='confirm',token=second))[1]
   assert rows("SELECT email_pending,auth_version FROM accounts WHERE handle='pending'")[0]==dict(email_pending=0,auth_version=2)
   assert not rows('SELECT * FROM account_email_verifications')
   call();assert 'invalid or expired' in call(data=dict(action='confirm',token=second))[1]
   assert api('account-client-login',login='pending@example.test',password=password)[0]==200
   assert api('account-client-logout',session=temporary_session)[0]==401
   source+=1
   unknown=call(data=dict(action='resend',email='missing@example.test'))[1]
   assert re.search(r'<p class="account-notice".*?</p>',notice)[0]==re.search(r'<p class="account-notice".*?</p>',unknown)[0]
   assert 'Too many attempts' in call(data=dict(action='resend',email='missing@example.test'))[1]
   # SMTP rejection rolls account and token creation back, including browser registration.
   source+=1;relay=Relay(reject=True);configure(True,relay)
   status,body=register('failure');relay.close();assert status==400 and 'could not be sent' in body
   assert not rows("SELECT * FROM accounts WHERE handle='failure'")
   source+=1;cookie='';call('/account/');relay=Relay(reject=True);configure(True,relay)
   page=call('/account/',dict(action='register',handle='webfail',email='webfail@example.test',password=password))[1];relay.close()
   assert 'could not be sent' in page and not rows("SELECT * FROM accounts WHERE handle='webfail'")
   source+=1;cookie='';call('/account/');relay=Relay();configure(True,relay)
   page=call('/account/',dict(action='register',handle='browser',email='browser@example.test',password=password))[1]
   browser_token=token(relay);assert 'Check your email' in page and '<h1>@browser' not in page
   sql("UPDATE accounts SET active=0 WHERE handle='browser'");call()
   assert 'invalid or expired' in call(data=dict(action='confirm',token=browser_token))[1]
   assert rows("SELECT active FROM accounts WHERE handle='browser'")[0]['active']==0
   source+=1;relay=Relay();configure(True,relay);assert register('changed')[0]==200
   changed_token=token(relay);sql("UPDATE accounts SET email='different@example.test' WHERE handle='changed'");call()
   assert 'invalid or expired' in call(data=dict(action='confirm',token=changed_token))[1]
   # Disabling the option restores immediate registration even without SMTP.
   source+=1;configure(False);assert register('disabled')[1].startswith('central-session ')
   print('PASS: configurable six-character passwords, SMTP prerequisite/rollback, pending browser/native/PIN/legacy gates, hash-only 24-hour activation, CSRF, single use, expiry/resend/limits, disabled-account/email binding, migration compatibility and runtime opt-out')
  finally:app.terminate();app.wait(timeout=5)
 finally:php('database()->exec("DROP DATABASE `".$argv[2]."`");',name)
