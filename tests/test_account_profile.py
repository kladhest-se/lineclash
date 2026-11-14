#!/usr/bin/env python3
"""Profile changes and irreversible live-data erasure, only in a fresh schema."""
import base64,gzip,hashlib,http.cookies,json,os,re,secrets,socket,subprocess,tempfile,time,urllib.request,urllib.error,urllib.parse
from pathlib import Path
from test_smtp import Relay
ROOT=Path(__file__).resolve().parents[1]
base=os.environ.get('LINECLASH_WEB_RESET_TEST_CONFIG')
if not base:
 print('SKIP: set LINECLASH_WEB_RESET_TEST_CONFIG to a test MariaDB config with CREATE DATABASE permission');raise SystemExit(0)
name='lineclash_profile_test_'+secrets.token_hex(8)
with tempfile.TemporaryDirectory(prefix='lineclash-profile-') as folder, tempfile.TemporaryFile() as log:
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
  def sql(statement,*parameters):return php('$db=database();$db->prepare($argv[2])->execute(json_decode($argv[3],true));',statement,json.dumps(parameters),environment=local)
  def rows(statement,*parameters):return json.loads(php('$q=database()->prepare($argv[2]);$q->execute(json_decode($argv[3],true));echo json_encode($q->fetchAll());',statement,json.dumps(parameters),environment=local))
  def configure(required=False,relay=None):
   smtp=dict(host='127.0.0.1',port=relay.port,encryption='none',from_address='sender@example.test') if relay else None
   php('$c=require web_config_path();$c["smtp"]=json_decode($argv[2],true);$c["account_require_email_verification"]=$argv[3]==="true";$c["account_password_min_length"]=6;$c["public_url"]="https://fixed.example.test";file_put_contents(web_config_path(),"<?php return ".var_export($c,true).";");',json.dumps(smtp),json.dumps(required),environment=local)
  cookie='';csrf='';source=10;password='secret';new_password='new-secret'
  with socket.socket() as s:s.bind(('127.0.0.1',0));port=s.getsockname()[1]
  app=subprocess.Popen(['php','-d','opcache.enable=0','-S',f'127.0.0.1:{port}','-t',str(ROOT/'public-web'),str(ROOT/'public-web/dev-router.php')],env=local,stdout=log,stderr=log)
  def call(path='/account/',data=None,api=False):
   global cookie,csrf
   headers={'Cookie':cookie,'X-Forwarded-For':f'198.18.44.{source}'};body=None
   if data is not None:
    headers['Content-Type']='application/json' if api else 'application/x-www-form-urlencoded'
    body=(json.dumps(data) if api else urllib.parse.urlencode(dict(csrf=csrf,**data))).encode()
   try:r=urllib.request.urlopen(urllib.request.Request(f'http://127.0.0.1:{port}'+path,body,headers),timeout=8)
   except urllib.error.HTTPError as e:r=e
   for value in r.headers.get_all('Set-Cookie',[]):
    c=http.cookies.SimpleCookie(value)
    if 'lineclash_account' in c:cookie='lineclash_account='+c['lineclash_account'].value
   text=r.read().decode();m=re.search(r'name="csrf" value="([a-f0-9]+)"',text)
   if m:csrf=m[1]
   return r.code,text
  def api(action,**fields):return call('/api/',dict(action=action,**fields),True)
  def signin(handle,pw=password):call();return call(data=dict(action='login',handle=handle,password=pw))[1]
  def get(path):status,body=call(path);assert status==200,(status,body);return json.loads(body)
  def email_token(relay):
   relay.close();assert len(relay.messages)==1
   message=base64.b64decode(relay.messages[0].split(b'\r\n\r\n')[1]).decode()
   return re.search(r'/account/email/#([a-f0-9]{64})',message)[1]
  try:
   configure()
   for _ in range(100):
    try:call();break
    except OSError:time.sleep(.03)
   identities={};native={}
   for handle in ('alice','bob','alice2'):
    status,body=api('account-client-register',format='session',handle=handle,email=handle+'@example.test',password=password);assert status==200,body
    native[handle]=body.split()[1];identities[handle]=rows('SELECT id FROM accounts WHERE handle=?',handle)[0]['id']
   assert 'Player settings' in signin('alice')
   assert 'current-password' in call()[1] and 'Permanently delete account' in call()[1]
   assert rows('SELECT avatar FROM accounts WHERE id=?',identities['alice'])[0]['avatar']=='initial'
   csrf='0'*64
   assert 'This form expired' in call(data=dict(action='avatar',avatar='t'))[1]
   assert 'available avatars' in call(data=dict(action='avatar',avatar='remote-image'))[1]
   assert rows('SELECT avatar FROM accounts WHERE id=?',identities['alice'])[0]['avatar']=='initial'
   page=call(data=dict(action='avatar',avatar='t',account_id=identities['bob']))[1]
   assert 'Avatar saved.' in page and 'value="t" checked' in page
   assert 'avatar-blocks' in call('/source/')[1].split('<main',1)[0]
   assert rows('SELECT avatar FROM accounts WHERE id=?',identities['alice'])[0]['avatar']=='t'
   assert rows('SELECT avatar FROM accounts WHERE id=?',identities['bob'])[0]['avatar']=='initial'

   sql('INSERT INTO account_email_verifications(account_id,token_hash,email,expires) VALUES(?,?,?,?)',identities['alice'],'a'*64,'alice@example.test',int(time.time()+86400))
   csrf='0'*64
   assert 'This form expired' in call(data=dict(action='password',current_password=password,new_password=new_password,confirm_password=new_password))[1]
   assert 'current password' in call(data=dict(action='password',current_password='wrong',new_password=new_password,confirm_password=new_password))[1]
   assert rows('SELECT account_id FROM account_email_verifications')
   assert 'do not match' in call(data=dict(action='password',current_password=password,new_password=new_password,confirm_password='different'))[1]
   assert 'Password changed.' in call(data=dict(action='password',current_password=password,new_password=new_password,confirm_password=new_password))[1]
   assert not rows('SELECT account_id FROM account_email_verifications')
   assert 'Your account: @alice' in call()[1]
   assert api('account-client-logout',session=native['alice'])[0]==401
   source+=1
   assert 'already in use' in call(data=dict(action='email',current_password=new_password,new_email='bob@example.test'))[1]
   assert 'Email changed.' in call(data=dict(action='email',current_password=new_password,new_email='changed@example.test'))[1]
   assert rows('SELECT email FROM accounts WHERE handle=?','alice')[0]['email']=='changed@example.test'
   assert api('account-client-login',login='alice@example.test',password=new_password)[0]==401
   # Email confirmation does not replace the current address until the POST succeeds.
   relay=Relay();configure(True,relay)
   page=call(data=dict(action='email',current_password=new_password,new_email='fresh@example.test'))[1];assert 'Check the new email' in page
   token=email_token(relay)
   assert rows('SELECT email FROM accounts WHERE handle=?','alice')[0]['email']=='changed@example.test'
   assert token not in json.dumps(rows('SELECT * FROM account_email_changes'))
   call('/account/email/');csrf='0'*64
   assert 'This form expired' in call('/account/email/',dict(token=token))[1]
   notification=Relay();configure(True,notification)
   assert 'Email changed.' in call('/account/email/',dict(token=token))[1];notification.close()
   assert b'To: <changed@example.test>' in notification.messages[0]
   call('/account/email/');assert 'invalid or expired' in call('/account/email/',dict(token=token))[1]
   configure(False);source+=1
   assert 'Your account: @alice' in signin('alice',new_password)
   status,body=api('account-client-login',login='fresh@example.test',password=new_password);assert status==200
   alice_session=body.split()[1]
   # A provisioned source reports three distinct, centrally authenticated players.
   server='fixture';key=secrets.token_hex(32);database=secrets.token_hex(16)
   sql("INSERT INTO directory_servers(id,name,host,port,token_hash,last_seen,status_json) VALUES(?,?,'127.0.0.1',31457,?,?,'{}')",server,'Fixture',hashlib.sha256(key.encode()).hexdigest(),int(time.time()))
   def game(number,winner='alice',team='Red'):
    events=[dict(elapsed_ms=0,line='playerjoin 1 @alice Red'),dict(elapsed_ms=0,line='playerjoin 2 @bob Blue'),dict(elapsed_ms=0,line='playerjoin 3 @alice2 Red'),dict(elapsed_ms=10,line='f 1 '+'0'*264),dict(elapsed_ms=20,line='f 2 '+'1'*264),dict(elapsed_ms=100,line='playerwon @'+winner),dict(elapsed_ms=100,line='endgame')]
    raw=json.dumps(events,separators=(',',':')).encode();order=[winner]+[h for h in ('alice','bob','alice2') if h!=winner]
    return dict(game_id=number,room_id=1,started_at='start',ended_at='end',started_epoch=1000,ended_epoch=1100,ranked=1,input_protocol='input-v1',end_reason='completed',winner_nick='@'+winner,winner_account_id=identities[winner],winner_team=team,player_count=3,replay_gzip=base64.b64encode(gzip.compress(raw)).decode(),replay_sha256=hashlib.sha256(raw).hexdigest(),players=[dict(nick='@'+h,account_id=identities[h],lines_cleared=10+i,specials_sent=i,blocks_placed=20+i,placement=order.index(h)+1,won=1 if h==winner else 0) for i,h in enumerate(('alice','bob','alice2'))])
   def upload(g):return call('/api/',dict(server_id=server,token=key,database_id=database,status={},games=[g]),True)
   for g in (game(1),game(2,'bob','Blue'),game(3,'alice2','Red')):assert upload(g)[0]==200
   profile=call()[1]
   for label,value in [('Matches played',3),('Wins',1),('Lines cleared',30),('Specials sent',0)]:
    assert '<dt>'+label+'</dt><dd>'+str(value)+'</dd>' in profile

   # An older replay-only reference contains escaped names and fields no longer permitted.
   legacy=json.dumps([dict(elapsed_ms=0,line='playerjoin 1 @alice fresh@example.test',private='fresh@example.test')]).replace('@alice','\\u0040alice')
   sql('UPDATE directory_games SET replay_json=?,replay_sha256=? WHERE game_id=3',legacy,hashlib.sha256(legacy.encode()).hexdigest())
   # Every private token family must be removed together with the account.
   version=rows('SELECT auth_version FROM accounts WHERE id=?',identities['alice'])[0]['auth_version']
   sql('INSERT INTO account_password_resets VALUES(?,?,?,?)',identities['alice'],'1'*64,version,9999999999)
   sql('INSERT INTO account_email_verifications VALUES(?,?,?,?)',identities['alice'],'2'*64,'fresh@example.test',9999999999)
   sql('INSERT INTO account_email_changes VALUES(?,?,?,?,?)',identities['alice'],'3'*64,'pending@example.test',version,9999999999)
   sql("INSERT INTO account_devices(request_hash,code,server_id,account_id,expires,state) VALUES(?,?,'fixture',?,9999999999,'approved')",'4'*64,'AABBCCDD',identities['alice'])
   sql("INSERT INTO account_client_pins(device_hash,pin,client,account_id,auth_version,expires,state) VALUES(?,?,'desktop',?,?,9999999999,'approved')",'5'*64,'12345678',identities['alice'],version)
   before=rows('SELECT COUNT(*) AS n,SUM(lines_cleared) AS cleared,SUM(specials_sent) AS specials,SUM(blocks_placed) AS blocks,SUM(won) AS wins FROM directory_players')[0]
   teams=get('/api/?action=teams')['teams']
   source+=1
   assert 'Type DELETE' in call(data=dict(action='delete',current_password=new_password,confirmation='no'))[1]
   assert 'current password' in call(data=dict(action='delete',current_password='wrong',confirmation='DELETE'))[1]
   # Rollback after scrubbing must restore names and credentials together.
   sql("CREATE TRIGGER delete_failure BEFORE DELETE ON accounts FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='fixture rollback'")
   assert 'Account deleted.' not in call(data=dict(action='delete',current_password=new_password,confirmation='DELETE'))[1]
   assert rows('SELECT nick FROM directory_players WHERE account_id=?',identities['alice'])
   sql('DROP TRIGGER delete_failure')
   assert 'Account deleted.' in call(data=dict(action='delete',current_password=new_password,confirmation='DELETE'))[1]
   assert not rows('SELECT * FROM accounts WHERE id=?',identities['alice'])
   for table in ('account_sessions','account_devices','account_client_pins','account_password_resets','account_email_verifications','account_email_changes'):
    assert not rows('SELECT * FROM '+table+' WHERE account_id=?',identities['alice'])
   after=rows('SELECT COUNT(*) AS n,SUM(lines_cleared) AS cleared,SUM(specials_sent) AS specials,SUM(blocks_placed) AS blocks,SUM(won) AS wins FROM directory_players')[0]
   assert before==after and get('/api/?action=teams')['teams']==teams
   assert 'Your account: @alice' not in call()[1]
   assert api('account-client-logout',session=alice_session)[0]==401
   assert call('/api/?action=player&server=fixture&nick=%40alice')[0]==404
   for number in (1,2,3):
    replay=get('/api/?action=replay&'+urllib.parse.urlencode(dict(server=server,database=database,game=number)))
    assert '@alice ' not in replay['replay_source'] and 'fresh@example.test' not in replay['replay_source']
    assert hashlib.sha256(replay['replay_source'].encode()).hexdigest()==replay['match']['replay_sha256']
    assert replay['match']['replay_redacted']==1
   first=get('/api/?action=replay&'+urllib.parse.urlencode(dict(server=server,database=database,game=1)))
   assert first['match']['winner_nick']=='DELETED PLAYER' and first['match']['winner_team']=='Red'
   assert next(p for p in first['players'] if p['deleted'])['won']==1
   assert any(p['nick']=='@alice2' and not p['deleted'] for p in first['players'])
   assert not any(p['winner_nick']=='@alice' for p in get('/api/?action=leaders')['leaders'])
   assert next(p['wins'] for p in get('/api/?action=leaders')['leaders'] if p['winner_deleted'])==1
   # Late uploads, including unsigned legacy identities, cannot restore an erased name.
   assert upload(game(4))[0]==200
   late=get('/api/?action=replay&'+urllib.parse.urlencode(dict(server=server,database=database,game=4)))
   assert late['match']['winner_deleted'] and late['match']['winner_nick']=='DELETED PLAYER'
   assert next(p['wins'] for p in get('/api/?action=leaders')['leaders'] if p['winner_deleted'])==2
   for path in ('/statistics/','/matches/','/leaderboards/','/directory/'):
    text=call(path)[1];assert '@alice<' not in text and 'Team leaderboards' in call('/statistics/')[1]
   # Deleting two participants in one match must preserve separate results and team wins.
   source+=1;assert 'Your account: @bob' in signin('bob')
   count=rows('SELECT COUNT(*) AS n FROM directory_players WHERE game_id=1')[0]['n'];teams=get('/api/?action=teams')['teams']
   assert 'Account deleted.' in call(data=dict(action='delete',current_password=password,confirmation='DELETE'))[1]
   assert rows('SELECT COUNT(*) AS n FROM directory_players WHERE game_id=1')[0]['n']==count
   assert rows('SELECT COUNT(DISTINCT nick) AS n FROM directory_players WHERE game_id=1 AND deleted=1')[0]['n']==2
   assert get('/api/?action=teams')['teams']==teams
   # A new owner of the same handle cannot inherit the old account's historical identity.
   source+=1;status,body=api('account-client-register',format='session',handle='alice',email='newowner@example.test',password=password);assert status==200
   new_id=rows('SELECT id FROM accounts WHERE handle=?','alice')[0]['id'];assert new_id!=identities['alice']
   assert upload(game(5))[0]==200
   stale=get('/api/?action=replay&'+urllib.parse.urlencode(dict(server=server,database=database,game=5)));assert stale['match']['winner_deleted']
   identities['alice']=new_id;assert upload(game(6))[0]==200
   fresh=get('/api/?action=replay&'+urllib.parse.urlencode(dict(server=server,database=database,game=6)));assert fresh['match']['winner_nick']=='@alice' and not fresh['match']['winner_deleted']
   # Private team labels keep a non-name grouping key across deletion and late uploads.
   assert upload(game(7,'alice','@alice crew'))[0]==200
   source+=1;assert 'Your account: @alice' in signin('alice')
   assert 'Account deleted.' in call(data=dict(action='delete',current_password=password,confirmation='DELETE'))[1]
   private_team=get('/api/?action=match&'+urllib.parse.urlencode(dict(server=server,database=database,game=7)))['match']['winner_team']
   assert private_team.startswith('DELETED TEAM ') and '@alice' not in private_team
   assert upload(game(8,'alice','@alice crew'))[0]==200
   repeated=get('/api/?action=match&'+urllib.parse.urlencode(dict(server=server,database=database,game=8)))['match']['winner_team']
   assert private_team==repeated
   assert next(team['wins'] for team in get('/api/?action=teams')['teams'] if team['winner_team']==private_team)==2
   print('PASS: saved allowlisted avatars, own-account match totals, reauthenticated profile changes, confirmed email, session revocation, atomic permanent deletion, replay-only and escaped references, valid replay hashes, separate anonymous results, preserved team wins, late-upload suppression and handle reuse isolation')
  finally:app.terminate();app.wait(timeout=5)
 finally:php('database()->exec("DROP DATABASE `".$argv[2]."`");',name)
