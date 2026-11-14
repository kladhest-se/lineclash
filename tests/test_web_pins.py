#!/usr/bin/env python3
"""Client-level PIN login, tested against a disposable MariaDB schema."""
import hashlib,http.cookies,json,os,re,secrets,socket,subprocess,tempfile,time,urllib.request,urllib.error,urllib.parse
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
base=os.environ.get('LINECLASH_WEB_RESET_TEST_CONFIG')
if not base:
 print('SKIP: set LINECLASH_WEB_RESET_TEST_CONFIG to a test MariaDB config with CREATE DATABASE permission')
 raise SystemExit(0)
name='lineclash_pin_test_'+secrets.token_hex(8)
with tempfile.TemporaryDirectory(prefix='lineclash-pin-') as folder, tempfile.TemporaryFile() as log:
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
  def sql(statement):return php('database()->exec($argv[2]);',statement,environment=local)
  def rows(statement):return json.loads(php('echo json_encode(database()->query($argv[2])->fetchAll());',statement,environment=local))
  with socket.socket() as s:s.bind(('127.0.0.1',0));port=s.getsockname()[1]
  app=subprocess.Popen(['php','-d','opcache.enable=0','-S',f'127.0.0.1:{port}','-t',str(ROOT/'public-web'),str(ROOT/'public-web/dev-router.php')],env=local,stdout=log,stderr=log)
  cookie='';csrf='';source=10
  def call(path='/pin/',data=None,api=False):
   global cookie,csrf
   headers={'Cookie':cookie,'X-Forwarded-For':f'198.18.22.{source}'};body=None
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
   return r.code,text
  def api(action,**fields):return call('/api/',dict(action=action,**fields),True)
  def begin(client='desktop'):
   global source
   source+=1
   status,body=api('account-client-pin-begin',client=client);assert status==200,body
   prefix,secret,pin,expires=body.split();assert prefix=='central-pin' and len(secret)==64 and len(pin)==8 and pin.isdigit() and 290<=int(expires)-time.time()<=300
   stored=rows("SELECT * FROM account_client_pins WHERE pin='"+pin+"'")[0]
   assert stored['device_hash']==hashlib.sha256(secret.encode()).hexdigest() and secret not in json.dumps(stored)
   return secret,pin
  def poll(secret):
   sql("UPDATE account_client_pins SET last_poll=0 WHERE device_hash='"+hashlib.sha256(secret.encode()).hexdigest()+"'")
   return api('account-client-pin-poll',device=secret)
  try:
   for _ in range(100):
    try:status,page=call();break
    except OSError:time.sleep(.03)
   assert status==200 and 'Enter the PIN shown' in page
   password=secrets.token_hex(16);email='pinuser@example.test'
   assert api('account-client-register',format='session',handle='pinuser',email=email,password=password)[0]==200
   secret,pin=begin();other,other_pin=begin('amiga')
   assert 'Enter the PIN shown' in call(data=dict(action='lookup',pin='bad'))[1]
   assert api('account-client-pin-poll',device=pin)[0]==400
   assert api('account-client-pin-poll',device='0'*64)[1].startswith('central-error ')
   assert poll(secret)==(200,'central-pending')
   assert api('account-client-pin-poll',device=secret)==(200,'central-slow-down')
   assert 'Username or email' in call(data=dict(action='lookup',pin=pin[:4]+' '+pin[4:]))[1]
   assert 'Sign-in details did not match' in call(data=dict(action='authenticate',pin=pin,login='pinuser',password='wrong'))[1]
   assert poll(secret)==(200,'central-pending')
   csrf='0'*64
   assert 'This form expired' in call(data=dict(action='authenticate',pin=pin,login='pinuser',password=password))[1]
   assert poll(secret)==(200,'central-pending')
   assert 'Client authenticated' in call(data=dict(action='authenticate',pin=pin,login='pinuser',password=password))[1]
   status,session=poll(secret);assert status==200 and session.startswith('central-session ') and session.endswith(' '+email),session
   assert poll(secret)[1].startswith('central-error ')
   assert poll(other)==(200,'central-pending')
   assert 'Amiga client' in call(data=dict(action='lookup',pin=other_pin))[1]
   sql("UPDATE account_client_pins SET state='denied' WHERE pin='"+other_pin+"'")
   assert poll(other)[1]=='central-error PIN sign-in declined'
   assert api('account-client-logout',session=session.split()[1])==(200,'central-signed-out')
   # Account menus have no approval UI; an existing browser session is insufficient.
   call('/account/');profile=call('/account/',dict(action='login',email=email,password=password))[1]
   assert 'Your account: @pinuser' in profile and 'Approve a game connection' not in profile and 'Sign in a client with a PIN' not in profile
   secret,pin=begin();page=call(data=dict(action='lookup',pin=pin))[1]
   assert 'Username or email' in page and 'Sign in and authenticate client' in page
   assert 'Sign-in details did not match' in call(data=dict(action='authenticate',pin=pin))[1]
   assert poll(secret)==(200,'central-pending')
   assert 'Client authenticated' in call(data=dict(action='authenticate',pin=pin,login=email.upper(),password=password))[1]
   sql('UPDATE accounts SET auth_version=auth_version+1')
   assert 'Account changed' in poll(secret)[1]
   secret,pin=begin();sql("UPDATE account_client_pins SET expires=1 WHERE pin='"+pin+"'")
   assert 'expired' in call(data=dict(action='lookup',pin=pin))[1]
   assert 'expired' in poll(secret)[1]
   assert 'Enter the PIN shown' in call('/account/pin/')[1]
   assert api('account-client-pin-begin',client='injected\nclient')[0]==400
   source+=1
   for _ in range(5):assert api('account-client-pin-begin')[0]==200
   assert api('account-client-pin-begin')[0]==429
   assert rows("SELECT COUNT(*) AS n FROM account_client_pins WHERE expires=1")[0]['n']==0
   print('PASS: hidden root PIN flow, PIN before fresh credentials, no cookie-only approval/menu links, hashed device secrets, single-use sessions, expiry, version revocation, legacy redirect and rate limits')
  finally:app.terminate();app.wait(timeout=5)
 finally:php('database()->exec("DROP DATABASE `".$argv[2]."`");',name)
