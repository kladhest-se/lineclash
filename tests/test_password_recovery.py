#!/usr/bin/env python3
"""Disposable MariaDB and local SMTP end-to-end password recovery tests."""
import base64,hashlib,http.cookies,json,os,re,secrets,socket,subprocess,tempfile,time,urllib.error,urllib.parse,urllib.request
from pathlib import Path
from test_smtp import Relay
ROOT=Path(__file__).resolve().parents[1]
base=os.environ.get('LINECLASH_WEB_RESET_TEST_CONFIG')
if not base:
 print('SKIP: set LINECLASH_WEB_RESET_TEST_CONFIG to a disposable MariaDB config with CREATE DATABASE permission')
 raise SystemExit(0)
name='lineclash_recovery_test_'+secrets.token_hex(8)
with tempfile.TemporaryDirectory(prefix='lineclash-recovery-') as folder, tempfile.TemporaryFile() as log:
 config=Path(folder)/'web.php';env=dict(os.environ,LINECLASH_WEB_CONFIG=base)
 def php(code,*args,environment=env):
  result=subprocess.run(['php','-r','require $argv[1]; '+code,str(ROOT/'public-web/inc/service.php'),*args],env=environment,text=True,capture_output=True)
  assert result.returncode==0,result.stderr
  return result.stdout
 php('database()->exec("CREATE DATABASE `".$argv[2]."` CHARACTER SET utf8mb4");',name)
 try:
  php('$c=require web_config_path();$c["dsn"]=preg_replace("/dbname=[^;]+/","dbname=".$argv[2],$c["dsn"]);file_put_contents($argv[3],"<?php return ".var_export($c,true).";");',name,str(config))
  local=dict(os.environ,LINECLASH_WEB_CONFIG=str(config))
  def sql(statement):return php('database()->exec($argv[2]);',statement,environment=local)
  def query(statement):return php('echo json_encode(database()->query($argv[2])->fetchAll());',statement,environment=local)
  subprocess.run(['php',str(ROOT/'public-web/bin/manage.php'),'migrate'],env=local,check=True,capture_output=True)
  def configure(smtp=None,url='https://fixed.example.test'):
   php('$c=require web_config_path();$c["smtp"]=json_decode($argv[2],true);$c["public_url"]=$argv[3];file_put_contents(web_config_path(),"<?php return ".var_export($c,true).";");',json.dumps(smtp),url,environment=local)
  def settings(relay):return dict(host='127.0.0.1',port=relay.port,encryption='none',from_address='sender@example.test',hello_host='localhost')
  with socket.socket() as s:s.bind(('127.0.0.1',0));port=s.getsockname()[1]
  cookie='';csrf='';peer='198.18.11.12'
  # Simulate trusted proxy addresses for isolated rate-limit assertions.
  php('$c=require web_config_path();$c["trusted_proxies"]=["127.0.0.1"];file_put_contents(web_config_path(),"<?php return ".var_export($c,true).";");',environment=local)
  app=subprocess.Popen(['php','-d','opcache.enable=0','-S',f'127.0.0.1:{port}','-t',str(ROOT/'public-web'),str(ROOT/'public-web/dev-router.php')],env=local,stdout=log,stderr=log)
  def call(path='/account/recover/',data=None,api=False):
   global cookie,csrf
   headers={'Cookie':cookie,'X-Forwarded-For':peer};body=None
   if data is not None:
    headers['Content-Type']='application/json' if api else 'application/x-www-form-urlencoded'
    body=(json.dumps(data) if api else urllib.parse.urlencode(dict(csrf=csrf,**data))).encode()
   request=urllib.request.Request(f'http://127.0.0.1:{port}'+path,body,headers)
   try:r=urllib.request.urlopen(request,timeout=5)
   except urllib.error.HTTPError as e:r=e
   for header in r.headers.get_all('Set-Cookie',[]):
    c=http.cookies.SimpleCookie(header)
    if 'lineclash_account' in c:cookie='lineclash_account='+c['lineclash_account'].value
   text=r.read().decode();m=re.search(r'name="csrf" value="([a-f0-9]+)"',text)
   if m:csrf=m[1]
   return r.code,text,r.headers
  try:
   for _ in range(100):
    try:call();break
    except OSError:time.sleep(.03)
   configure()
   assert 'disabled because SMTP' in call()[1]
   assert 'name="email"' not in call()[1]
   page=call('/account/')[1]
   assert all(link in page for link in ('Create an account','>Sign in</a>','Resend activation email','href="recover/">Forgot password?'))
   for invalid_url in ('http://bad.example','https://user:pass@example.test','https://example.test/?bad=1'):
    configure(dict(host='localhost',from_address='sender@example.test'),invalid_url)
    assert 'disabled because SMTP' in call()[1]
   handle='u'+secrets.token_hex(4);email=handle+'@example.test';password=secrets.token_hex(16);newpass=secrets.token_hex(16)
   status,body,_=call('/api/',dict(action='account-client-register',format='session',handle=handle,email=email,password=password),True)
   assert status==200 and body.startswith('central-session '),body
   native=body.split()[1]
   # An approved game connection must also become unusable after a password reset.
   server_key=secrets.token_hex(32);nonce=secrets.token_hex(32)
   account_id=json.loads(query('SELECT id FROM accounts'))[0]['id']
   sql("INSERT INTO directory_servers(id,name,host,port,token_hash,last_seen,status_json) VALUES('recovery-fixture','Fixture','127.0.0.1',31457,'"+hashlib.sha256(server_key.encode()).hexdigest()+"',"+str(int(time.time()))+",'{}')")
   sql("INSERT INTO account_devices(request_hash,code,server_id,account_id,expires,state) VALUES('"+hashlib.sha256(nonce.encode()).hexdigest()+"','ABCDEF12','recovery-fixture','"+account_id+"',"+str(int(time.time())+300)+",'approved')")
   call('/account/');assert '@'+handle in call('/account/',dict(action='login',email=email,password=password))[1]
   # Disabled endpoint cannot issue links or modify passwords, even with a forged POST.
   assert 'disabled because SMTP' in call(data=dict(action='request',email=email))[1]
   assert json.loads(query('SELECT * FROM account_password_resets'))==[]
   browser_cookie=cookie;cookie=''
   relay=Relay();configure(settings(relay));call()
   assert 'Send reset link' in call()[1]
   call();oldcsrf=csrf;csrf='0'*64
   assert 'This form expired' in call(data=dict(action='request',email=email))[1];csrf=oldcsrf
   body=call(data=dict(action='request',email=email))[1];assert 'If an active account' in body
   relay.close();assert len(relay.messages)==1
   message=base64.b64decode(relay.messages[0].split(b'\r\n\r\n')[1]).decode()
   token=re.search(r'https://fixed.example.test/account/recover/#([a-f0-9]{64})',message)[1]
   row=json.loads(query('SELECT token_hash,expires FROM account_password_resets'))[0]
   assert row['token_hash']==hashlib.sha256(token.encode()).hexdigest() and token not in query('SELECT * FROM account_password_resets')
   assert 1700<int(row['expires'])-time.time()<=1800
   status,page,headers=call();assert headers['Cache-Control']=='no-store' and headers['Referrer-Policy']=='no-referrer'
   assert 'name="recovery"' not in page and token not in page
   assert 'Passwords do not match' in call(data=dict(action='reset',token=token,password=newpass,confirm='wrong'))[1]
   assert 'Password is too short' in call(data=dict(action='reset',token=token,password='x',confirm='x'))[1]
   assert 'invalid or expired' in call(data=dict(action='reset',token='0'*64,password=newpass,confirm=newpass))[1]
   assert 'Password changed.' in call(data=dict(action='reset',token=token,password=newpass,confirm=newpass))[1]
   call() # A successful reset rotates the CSRF token as well as the browser session.
   assert 'invalid or expired' in call(data=dict(action='reset',token=token,password=newpass,confirm=newpass))[1]
   cookie=browser_cookie
   assert '@'+handle not in call('/account/')[1] # Prior browser session invalidated.
   assert call('/api/',dict(action='account-client-login',login=email,password=password),True)[0]==401
   assert call('/api/',dict(action='account-client-login',login=email,password=newpass),True)[0]==200
   assert call('/api/',dict(action='account-client-logout',session=native),True)[0]==401
   assert json.loads(query('SELECT * FROM account_password_resets'))==[]
   assert call('/api/',dict(action='account-poll',server_key=server_key,nonce=nonce),True)[1].startswith('central-error ')
   # Unknown accounts and SMTP failures both have the same public acknowledgement.
   peer='198.18.11.13';call();unknown=call(data=dict(action='request',email='nobody@example.test'))[1]
   assert 'If an active account' in unknown
   sql('DELETE FROM directory_registration_limits')
   relay=Relay(reject=True);configure(settings(relay));call();failed=call(data=dict(action='request',email=email))[1];relay.close()
   assert re.search(r'<p class="account-notice".*?</p>',failed)[0]==re.search(r'<p class="account-notice".*?</p>',unknown)[0]
   assert not json.loads(query('SELECT * FROM account_password_resets'))
   # A newer email supersedes the previous link; expired links and disablement reject POST.
   tokens=[]
   for _ in range(2):
    sql('DELETE FROM directory_registration_limits');relay=Relay();configure(settings(relay));call();call(data=dict(action='request',email=email));relay.close()
    tokens.append(re.search(r'/account/recover/#([a-f0-9]{64})',base64.b64decode(relay.messages[0].split(b'\r\n\r\n')[1]).decode())[1])
   assert 'invalid or expired' in call(data=dict(action='reset',token=tokens[0],password=password,confirm=password))[1]
   sql('UPDATE account_password_resets SET expires=1')
   assert 'invalid or expired' in call(data=dict(action='reset',token=tokens[1],password=password,confirm=password))[1]
   configure();assert 'disabled because SMTP' in call(data=dict(action='reset',token=tokens[1],password=password,confirm=password))[1]
   sql('DELETE FROM directory_registration_limits');configure(dict(host='localhost',port=1,encryption='none',from_address='sender@example.test'))
   call();assert 'If an active account' in call(data=dict(action='request',email='missing@example.test'))[1]
   assert 'Too many attempts' in call(data=dict(action='request',email='missing@example.test'))[1]
   print('PASS: SMTP disabled/configured states, fixed-origin private links, CSRF, hashed/expiring/single-use tokens, password policy, account/session invalidation, delivery failure, generic acknowledgement, superseded links and rate limits')
  finally:app.terminate();app.wait(timeout=5)
 finally:php('database()->exec("DROP DATABASE `".$argv[2]."`");',name)
