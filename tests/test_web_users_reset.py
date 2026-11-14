#!/usr/bin/env python3
"""Exercise user-only reset and SQL recovery in a newly created disposable schema."""
import hashlib,json,os,secrets,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
base=os.environ.get('LINECLASH_WEB_RESET_TEST_CONFIG')
if not base:
 print('SKIP: set LINECLASH_WEB_RESET_TEST_CONFIG to a disposable MariaDB configuration')
 raise SystemExit(0)
name='lineclash_users_test_'+secrets.token_hex(8)
tables=['account_email_changes','account_email_verifications','account_client_pins','account_password_resets','account_sessions','account_devices','directory_players','directory_games','directory_registration_limits','accounts','directory_servers','unrelated_notes']
with tempfile.TemporaryDirectory(prefix='lineclash-users-') as folder:
 folder=Path(folder);config=folder/'web.php';env=dict(os.environ,LINECLASH_WEB_CONFIG=base)
 def php(code,*args,environment=env):
  result=subprocess.run(['php','-r','require $argv[1]; '+code,str(ROOT/'public-web/inc/service.php'),*args],env=environment,text=True,capture_output=True)
  assert result.returncode==0,result.stderr
  return result.stdout
 php('database()->exec("CREATE DATABASE `".$argv[2]."` CHARACTER SET utf8mb4");',name)
 try:
  php('$c=require web_config_path();$c["dsn"]=preg_replace("/dbname=[^;]+/","dbname=".$argv[2],$c["dsn"],1,$n);if($n!==1)exit(1);file_put_contents($argv[3],"<?php return ".var_export($c,true).";");chmod($argv[3],0600);',name,str(config))
  local=dict(os.environ,LINECLASH_WEB_CONFIG=str(config))
  def manage(*args):return subprocess.run(['php',str(ROOT/'public-web/bin/manage.php'),*args],env=local,text=True,capture_output=True)
  def sql(statement):return php('database()->exec($argv[2]);',statement,environment=local)
  def snapshot():return json.loads(php('$out=[];foreach(json_decode($argv[2],true) as $t)$out[$t]=database()->query("SELECT * FROM `".$t."`")->fetchAll();echo json_encode($out);',json.dumps(tables),environment=local))
  result=manage('migrate');assert result.returncode==0,result.stderr
  sql("INSERT INTO directory_servers(id,name,host,port,token_hash,status_json) VALUES('fixture','Fixture','127.0.0.1',31457,'"+'0'*64+"','{}')")
  for char,handle in [('a','alice'),('c','bob')]:
   sql(f"INSERT INTO accounts(id,handle,email,password_hash,recovery_hash,created_at) VALUES('{char*32}','{handle}','{handle}@example.test','fixture','{'1'*64}',1)")
  uid='a'*32;dbid='b'*32
  sql(f"INSERT INTO account_email_changes VALUES('{uid}','{'8'*64}','pending@example.test',1,9999999999)")
  sql(f"INSERT INTO account_email_verifications VALUES('{uid}','{'7'*64}','alice@example.test',9999999999)")
  sql(f"INSERT INTO account_client_pins(device_hash,pin,client,account_id,auth_version,expires) VALUES('{'6'*64}','12345678','desktop','{uid}',1,9999999999)")
  sql(f"INSERT INTO account_password_resets VALUES('{uid}','{'5'*64}',1,9999999999)")
  sql(f"INSERT INTO account_sessions(token_hash,account_id,auth_version,expires) VALUES('{'2'*64}','{uid}',1,9999999999)")
  sql(f"INSERT INTO account_devices(request_hash,code,server_id,account_id,expires,state) VALUES('{'3'*64}','01234567','fixture','{uid}',9999999999,'pending')")
  sql(f"INSERT INTO directory_games(server_id,database_id,game_id,room_id,started_at,ended_at,winner_nick,winner_team,player_count,winner_account_id) VALUES('fixture','{dbid}',1,1,'start','end','@alice','@alice squad',2,'{uid}')")
  for char,handle,placement in [('a','alice',1),('c','bob',2)]:
   sql(f"INSERT INTO directory_players(server_id,database_id,game_id,nick,lines_cleared,specials_sent,blocks_placed,placement,won,account_id) VALUES('fixture','{dbid}',1,'@{handle}',12,3,40,{placement},{int(placement==1)},'{char*32}')")
  replay=json.dumps([dict(elapsed_ms=0,line='playerjoin 1 @alice alice@example.test'),dict(elapsed_ms=1,line='playerjoin 2 @bob -'),dict(elapsed_ms=2,line="chat @alice Hello 'world' \\ music 🎵")])
  php('database()->prepare("UPDATE directory_games SET replay_json=?,replay_sha256=?")->execute([$argv[2],hash("sha256",$argv[2])]);',replay,environment=local)
  sql(f"INSERT INTO directory_registration_limits VALUES('{'4'*64}',1,1)")
  sql('CREATE TABLE unrelated_notes(id INT PRIMARY KEY) ENGINE=InnoDB');sql('INSERT INTO unrelated_notes VALUES(1)')
  before=snapshot();original=config.read_bytes()
  for args in [('reset-users',),('reset-users','--dry-run')]:
   result=manage(*args);assert result.returncode==0 and 'Preview only' in result.stdout,result.stderr
   assert snapshot()==before
  for args in [('reset-users','--confirm-reset'),('reset-users','--backup',str(folder/'missing.sql')),('reset-users','--confirm-reset','--backup',str(folder/'no-parent'/'backup.sql'))]:
   assert manage(*args).returncode!=0 and snapshot()==before
  # Neither existing files nor backups under the configured document root are accepted.
  occupied=folder/'existing.sql';occupied.write_text('keep')
  result=manage('reset-users','--confirm-reset','--backup',str(occupied));assert result.returncode!=0 and occupied.read_text()=='keep'
  nested=folder/'website-backup.sql';result=manage('reset-users','--confirm-reset','--backup',str(nested));assert result.returncode!=0 and not nested.exists()
  # Backups live outside the config/document root, including when using staged code.
  with tempfile.TemporaryDirectory(prefix='lineclash-private-backup-') as private:
   private=Path(private)
   sql('ALTER TABLE directory_registration_limits ENGINE=MyISAM')
   result=manage('reset-users','--confirm-reset','--backup',str(private/'engine.sql'));assert result.returncode!=0 and 'InnoDB' in result.stderr
   sql('ALTER TABLE directory_registration_limits ENGINE=InnoDB')
   sql("CREATE TRIGGER users_failure BEFORE DELETE ON accounts FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='fixture failure'")
   failed=private/'rollback.sql';result=manage('reset-users','--confirm-reset','--backup',str(failed))
   assert result.returncode!=0 and 'rolled back' in result.stderr,result.stderr
   assert snapshot()==before,'All deletions and replay redactions must roll back together'
   assert failed.is_file() and failed.stat().st_mode&0o777==0o600
   sql('DROP TRIGGER users_failure')
   backup=private/'users.sql';result=manage('reset-users','--confirm-reset','--backup',str(backup))
   assert result.returncode==0 and 'Removed 2 central accounts' in result.stdout,result.stderr
   after=snapshot()
   for table in tables:
    if table.startswith('account') or table=='directory_registration_limits':assert after[table]==[],table
   assert after['directory_servers']==before['directory_servers'] and after['unrelated_notes']==before['unrelated_notes']
   assert len(after['directory_games'])==1 and len(after['directory_players'])==2
   for row in after['directory_players']:
    assert row['account_id'] is None and int(row['deleted'])==1
    assert (int(row['lines_cleared']),int(row['specials_sent']),int(row['blocks_placed']))==(12,3,40)
   assert sorted((int(p['placement']),int(p['won'])) for p in after['directory_players'])==[(1,1),(2,0)]
   game=after['directory_games'][0];assert game['winner_account_id'] is None and int(game['winner_deleted'])==1
   assert game['winner_team'].startswith('DELETED TEAM ') and int(game['replay_redacted'])==1
   assert 'alice' not in json.dumps(after) and 'bob' not in json.dumps(after) and '@example.test' not in json.dumps(after)
   assert hashlib.sha256(game['replay_json'].encode()).hexdigest()==game['replay_sha256']
   assert backup.stat().st_mode&0o777==0o600 and config.read_bytes()==original
   # Restore the emitted SQL to this same disposable schema, exercising FK order,
   # Unicode, quotes and backslashes, without changing unrelated tables or schema.
   php('database()->exec(file_get_contents($argv[2]));',str(backup),environment=local)
   restored=snapshot()
   for table in tables:
    assert sorted(json.dumps(r,sort_keys=True) for r in restored[table])==sorted(json.dumps(r,sort_keys=True) for r in before[table]),table
  print('PASS: user reset preview, private restorable backup, atomic rollback, anonymous results, retained servers and unrelated data')
 finally:
  php('database()->exec("DROP DATABASE `".$argv[2]."`");',name)
