#!/usr/bin/env python3
"""Disposable PHP/MariaDB central-account and single-use approval workflow."""
import http.cookies,json,os,re,secrets,socket,subprocess,tempfile,time,urllib.request,urllib.error
from pathlib import Path
root=Path(__file__).resolve().parents[1];assert os.environ.get('LINECLASH_WEB_CONFIG')
subprocess.run(['php',str(root/'public-web/bin/manage.php'),'migrate'],check=True)
server='account_test_'+secrets.token_hex(8);handle='u'+secrets.token_hex(5);password=secrets.token_hex(16)
output=subprocess.check_output(['php',str(root/'public-web/bin/manage.php'),'server',server,'127.0.0.1','31457','Account test server'],text=True);key=output.split()[-1]
with socket.socket() as s:s.bind(('127.0.0.1',0));port=s.getsockname()[1]
cookie='';csrf=''
with tempfile.TemporaryFile() as log:
 p=subprocess.Popen(['php','-S',f'127.0.0.1:{port}','-t',str(root/'public-web'),str(root/'public-web/dev-router.php')],stdout=log,stderr=log)
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
  print('PASS: central registration, browser session, profile, login/logout, server-bound approval and single-use redemption')
 finally:p.terminate();p.wait(timeout=5)
