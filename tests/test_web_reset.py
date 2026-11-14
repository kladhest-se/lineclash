#!/usr/bin/env python3
"""Test destructive resets only in a newly created, disposable MariaDB schema."""
import json,os,secrets,subprocess,tempfile,socket,time,urllib.request,urllib.error
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
base=os.environ.get('LINECLASH_WEB_RESET_TEST_CONFIG')
if not base:
 print('SKIP: set LINECLASH_WEB_RESET_TEST_CONFIG to a test MariaDB config with CREATE DATABASE permission')
 raise SystemExit(0)
name='lineclash_reset_test_'+secrets.token_hex(8)
tables=['account_client_pins','account_password_resets','account_sessions','account_devices','directory_players','directory_games','directory_registration_limits','accounts','directory_servers']
with tempfile.TemporaryDirectory(prefix='lineclash-reset-') as folder:
 config=Path(folder)/'web.php';env=dict(os.environ,LINECLASH_WEB_CONFIG=base)
 def php(code,*args,environment=env):
  result=subprocess.run(['php','-r','require $argv[1]; '+code,str(ROOT/'public-web/inc/service.php'),*args],env=environment,text=True,capture_output=True)
  assert result.returncode==0,result.stderr
  return result.stdout
 # Only this random, newly created schema is ever deleted by this test.
 php('$db=database();$db->exec("CREATE DATABASE `".$argv[2]."` CHARACTER SET utf8mb4");',name)
 try:
  php('$c=require web_config_path();$c["dsn"]=preg_replace("/dbname=[^;]+/","dbname=".$argv[2],$c["dsn"],1,$count);if($count!==1)exit(1);file_put_contents($argv[3],"<?php return ".var_export($c,true).";");chmod($argv[3],0600);',name,str(config))
  local=dict(os.environ,LINECLASH_WEB_CONFIG=str(config))
  def manage(*args):return subprocess.run(['php',str(ROOT/'public-web/bin/manage.php'),*args],env=local,text=True,capture_output=True)
  result=manage('migrate');assert result.returncode==0,result.stderr
  def sql(statement):return php('database()->exec($argv[2]);',statement,environment=local)
  def counts():return json.loads(php('$result=[];foreach(array_merge(json_decode($argv[2],true),["unrelated_notes"]) as $table)$result[$table]=(int)database()->query("SELECT COUNT(*) FROM `".$table."`")->fetchColumn();echo json_encode($result);',json.dumps(tables),environment=local))
  # Reproduce the production failure in this empty temporary schema only.
  sql('ALTER TABLE accounts DROP INDEX accounts_email, DROP COLUMN email')
  with socket.socket() as sock:sock.bind(('127.0.0.1',0));port=sock.getsockname()[1]
  with tempfile.TemporaryFile() as log:
   api=subprocess.Popen(['php','-S',f'127.0.0.1:{port}','-t',str(ROOT/'public-web'),str(ROOT/'public-web/dev-router.php')],env=local,stdout=log,stderr=log)
   try:
    for _ in range(100):
     try:
      with socket.create_connection(('127.0.0.1',port),.1):break
     except OSError:time.sleep(.03)
    payload=json.dumps(dict(action='account-client-login',login='diagnostic@example.invalid',password='')).encode()
    req=urllib.request.Request(f'http://127.0.0.1:{port}/api/',payload,{'Content-Type':'application/json'})
    try:urllib.request.urlopen(req,timeout=5);raise AssertionError('Missing email column should fail')
    except urllib.error.HTTPError as error:
     body=error.read().decode();assert error.code==503 and body.startswith('central-error Website database migration required'),body
    result=manage('migrate');assert result.returncode==0,result.stderr
    try:urllib.request.urlopen(req,timeout=5);raise AssertionError('Diagnostic login must not exist')
    except urllib.error.HTTPError as error:assert error.code==401
   finally:api.terminate();api.wait(timeout=5)
  sql('DELETE FROM directory_registration_limits')
  sql("INSERT INTO directory_servers(id,name,host,port,token_hash,status_json) VALUES('fixture','Fixture','127.0.0.1',31457,'"+'0'*64+"','{}')")
  sql("INSERT INTO accounts(id,handle,password_hash,recovery_hash,created_at) VALUES('"+'a'*32+"','tester','fixture','"+'1'*64+"',1)")
  sql("INSERT INTO account_client_pins(device_hash,pin,client,account_id,auth_version,expires) VALUES('"+'6'*64+"','12345678','desktop','"+'a'*32+"',1,9999999999)")
  sql("INSERT INTO account_password_resets VALUES('"+'a'*32+"','"+'5'*64+"',1,9999999999)")
  sql("INSERT INTO account_sessions(token_hash,account_id,auth_version,expires) VALUES('"+'2'*64+"','"+'a'*32+"',1,9999999999)")
  sql("INSERT INTO account_devices(request_hash,code,server_id,account_id,expires,state) VALUES('"+'3'*64+"','01234567','fixture','"+'a'*32+"',9999999999,'pending')")
  sql("INSERT INTO directory_games(server_id,database_id,game_id,room_id,started_at,ended_at,winner_nick,winner_team,player_count) VALUES('fixture','"+'b'*32+"',1,1,'start','end','tester','',1)")
  sql("INSERT INTO directory_players VALUES('fixture','"+'b'*32+"',1,'tester',1,1,1,1,1)")
  sql("INSERT INTO directory_registration_limits VALUES('"+'4'*64+"',1,1)")
  sql('CREATE TABLE unrelated_notes(id INT PRIMARY KEY) ENGINE=InnoDB');sql('INSERT INTO unrelated_notes VALUES(1)')
  before=counts();assert all(n==1 for n in before.values())
  original=config.read_bytes()
  for args in [('reset-data',),('reset-data','--dry-run')]:
   result=manage(*args);assert result.returncode==0 and 'Preview only' in result.stdout and name in result.stdout,result.stderr
   assert counts()==before
  result=manage('reset-data','--confirm-reset','--dry-run');assert result.returncode!=0 and counts()==before
  sql('ALTER TABLE directory_registration_limits ENGINE=MyISAM')
  result=manage('reset-data','--confirm-reset');assert result.returncode!=0 and 'InnoDB' in result.stderr and counts()==before
  sql('ALTER TABLE directory_registration_limits ENGINE=InnoDB')
  sql("CREATE TRIGGER reset_failure BEFORE DELETE ON directory_games FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='fixture failure'")
  result=manage('reset-data','--confirm-reset');assert result.returncode!=0 and 'rolled back' in result.stderr,result.stderr
  assert counts()==before,'Partial reset was not rolled back'
  sql('DROP TRIGGER reset_failure')
  result=manage('reset-data','--confirm-reset');assert result.returncode==0 and 'Removed 9 rows' in result.stdout,result.stderr
  after=counts();assert all(after[t]==0 for t in tables) and after['unrelated_notes']==1
  assert config.read_bytes()==original
  print('PASS: reset preview, exact confirmation, engine check, atomic rollback, all project tables emptied, unrelated data and configuration preserved')
 finally:
  php('$db=database();$db->exec("DROP DATABASE `".$argv[2]."`");',name)
