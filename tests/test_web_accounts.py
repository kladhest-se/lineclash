#!/usr/bin/env python3
"""Disposable PHP/MariaDB central-account and single-use approval workflow."""
import hashlib,http.cookies,json,os,re,secrets,socket,subprocess,tempfile,time,urllib.request,urllib.error
from pathlib import Path
root=Path(__file__).resolve().parents[1];assert os.environ.get('LINECLASH_WEB_CONFIG')
subprocess.run(['php',str(root/'public-web/bin/manage.php'),'migrate'],check=True)
server='account_test_'+secrets.token_hex(8);handle='u'+secrets.token_hex(5);password=secrets.token_hex(16)
output=subprocess.check_output(['php',str(root/'public-web/bin/manage.php'),'server',server,'127.0.0.1','31457','Account test server'],text=True);key=output.split()[-1]
with socket.socket() as s:s.bind(('127.0.0.1',0));port=s.getsockname()[1]
cookie='';csrf=''
with tempfile.TemporaryDirectory(prefix='lineclash-account-config-') as config_dir, tempfile.TemporaryFile() as log:
 policy_file=Path(config_dir)/'web.php'
 def policy_minimum(value):policy_file.write_text('<?php return array_replace(require getenv("LINECLASH_WEB_TEST_BASE_CONFIG"), ["account_password_min_length"=>'+str(value)+']);')
 policy_minimum(12)
 test_env=dict(os.environ,LINECLASH_WEB_TEST_BASE_CONFIG=os.environ['LINECLASH_WEB_CONFIG'],LINECLASH_WEB_CONFIG=str(policy_file))
 p=subprocess.Popen(['php','-S',f'127.0.0.1:{port}','-t',str(root/'public-web'),str(root/'public-web/dev-router.php')],stdout=log,stderr=log,env=test_env)
 def call(path='/account/',data=None,api=False):
  global cookie,csrf
  headers={'Cookie':cookie};body=None
  if data is not None:
   if api:headers['Content-Type']='application/json';body=json.dumps(data).encode()
   else:headers['Content-Type']='application/x-www-form-urlencoded';body=urllib.parse.urlencode(dict(csrf=csrf,**data)).encode()
  req=urllib.request.Request(f'http://127.0.0.1:{port}'+path,body,headers)
  try:r=urllib.request.urlopen(req,timeout=4)
  except urllib.error.HTTPError as e:r=e
  for value in r.headers.get_all('Set-Cookie',[]):
   c=http.cookies.SimpleCookie(value)
   if 'lineclash_account' in c:cookie='lineclash_account='+c['lineclash_account'].value
  result=r.read().decode();match=re.search(r'name="csrf" value="([a-f0-9]+)"',result)
  if match:csrf=match[1]
  return r.code,result
 try:
  for _ in range(100):
   try:call();break
   except OSError:time.sleep(.03)
  assert csrf
  page=call()[1]
  assert 'novalidate' in page and 'Use at least 12 characters.' in page
  assert 'aria-describedby="account-password-help"' in page
  short=secrets.token_hex(5)+'x'
  invalid=call(data=dict(action='register',handle=handle,password=short))[1]
  assert 'Password is too short. Use at least 12 characters.' in invalid
  assert 'role="alert"' in invalid and 'Save your recovery code' not in invalid
  assert 'value="'+handle+'"' in invalid and short not in invalid
  too_long=call(data=dict(action='register',handle=handle,password='é'*37))[1]
  assert 'Password is too long.' in too_long and 'Save your recovery code' not in too_long
  assert 'Save your recovery code' in call(data=dict(action='register',handle=handle,password=password))[1]
  assert '@'+handle in call()[1]
  assert call('/api/',dict(server_id=server,token=key,database_id=secrets.token_hex(16),games=[],status={}),True)[0]==200
  nonce=secrets.token_hex(32);request=dict(server_key=key,nonce=nonce)
  code,body=call('/api/',dict(request,action='account-begin'),True);assert code==200,(code,body)
  device=body.split()[1];assert len(device)==8
  assert call('/api/',dict(request,action='account-poll'),True)[1]=='central-pending'
  assert 'Account test server' in call('/account/?code='+device)[1]
  csrf='0'*64
  assert 'This form expired' in call(data=dict(action='approve',code=device))[1]
  assert call('/api/',dict(request,action='account-poll'),True)[1]=='central-pending'
  assert 'Approved. Return to the game.' in call(data=dict(action='approve',code=device))[1]
  body=call('/api/',dict(request,action='account-poll'),True)[1];assert body.startswith('central-approved ') and body.endswith(' '+handle),body
  assert call('/api/',dict(request,action='account-poll'),True)[1].startswith('central-error')
  assert call('/api/',dict(request,action='account-poll',server_key='f'*64),True)[0]==403
  call(data=dict(action='logout'));assert 'Sign in. Play anywhere.' in call()[1]
  call(data=dict(action='login',handle=handle,password=password));assert '@'+handle in call()[1]
  # Native profiles never send a browser session or a password to the game server.
  request_login=dict(action='account-client-login',handle=handle,password=password)
  status,bad=call('/api/',dict(request_login,password='incorrect-but-long'),True)
  assert status==401 and bad=='central-error Incorrect username or password'
  status,body=call('/api/',request_login,True);assert status==200,(status,body)
  _,session,returned,expires=body.split();assert returned==handle and len(session)==64
  def begin():
   nonce=secrets.token_hex(32);auth=dict(server_key=key,nonce=nonce)
   status,body=call('/api/',dict(auth,action='account-begin',direct=True),True)
   assert status==200 and body=='central-challenge '+hashlib.sha256(nonce.encode()).hexdigest(),(status,body)
   return auth,body.split()[1]
  auth,challenge=begin()
  join=dict(action='account-client-join',session=session,challenge=challenge,host='127.0.0.1',port=31457)
  assert call('/api/',dict(join,host='203.0.113.55'),True)[0]==403
  assert call('/api/',dict(join,port=31458),True)[0]==403
  assert call('/api/',dict(auth,action='account-poll'),True)[1]=='central-pending'
  assert call('/api/',dict(join,session='0'*64),True)[0]==401
  assert call('/api/',join,True)==(200,'central-joined '+handle)
  assert call('/api/',dict(auth,action='account-poll'),True)[1].endswith(' '+handle)
  assert call('/api/',join,True)[0]==403  # Already approved/consumed.
  # Another independent session can sign in to the same central identity.
  status,other=call('/api/',request_login,True);assert status==200
  other_session=other.split()[1];assert other_session!=session
  assert call('/api/',dict(action='account-client-logout',session=session),True)==(200,'central-signed-out')
  auth,challenge=begin();join.update(challenge=challenge)
  assert call('/api/',join,True)[0]==401
  join['session']=other_session
  assert call('/api/',join,True)==(200,'central-joined '+handle)
  assert call('/api/',dict(auth,action='account-poll'),True)[1].endswith(' '+handle)
  # Password recovery invalidates client sessions as well as browser sessions.
  php='require '+json.dumps(str(root/'public-web/inc/service.php'))+'; $q=database()->prepare("UPDATE accounts SET auth_version=auth_version+1 WHERE handle=?");$q->execute([$argv[1]]);'
  subprocess.run(['php','-r',php,handle],check=True)
  auth,challenge=begin();join.update(challenge=challenge)
  assert call('/api/',join,True)[0]==401
  policy_minimum(6)
  assert 'minlength="6"' in call()[1] and 'Use at least 6 characters.' in call()[1]
  short_handle='v'+secrets.token_hex(5);short_password=secrets.token_hex(3)
  assert 'Use at least 6 characters.' in call(data=dict(action='register',handle=short_handle,password=short_password[:5]))[1]
  page=call(data=dict(action='register',handle=short_handle,password=short_password))[1]
  assert 'Save your recovery code' in page
  recovery=re.search(r'<code>([a-f0-9]{48})</code>',page).group(1)
  policy_minimum(20)
  call(data=dict(action='logout'))
  assert '@'+short_handle in call(data=dict(action='login',handle=short_handle,password=short_password))[1]
  status,native=call('/api/',dict(action='account-client-login',handle=short_handle,password=short_password),True)
  assert status==200
  native_session=native.split()[1]
  assert 'Use at least 20 characters.' in call(data=dict(action='recover',handle=short_handle,password=short_password,recovery=recovery))[1]
  assert 'Save your recovery code' in call(data=dict(action='recover',handle=short_handle,password=secrets.token_hex(16),recovery=recovery))[1]
  assert call('/api/',dict(action='account-client-logout',session=native_session),True)[0]==401
  print('PASS: visible validation, rejected short/oversized passwords, central registration, browser sessions, native profile sessions, target-bound HTTPS joins, logout/recovery revocation, single-use redemption and configurable password policy with existing-account login')
 finally:p.terminate();p.wait(timeout=5)
