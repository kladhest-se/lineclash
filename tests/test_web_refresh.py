#!/usr/bin/env python3
"""Offline deployment-script tests. External commands are isolated fixtures."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
@unittest.skipUnless(os.name!="nt" and shutil.which("php"), "Deployment helper tests require POSIX shell and PHP CLI")
class RefreshTests(unittest.TestCase):
 def setUp(self):
  self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup);self.base=Path(self.tmp.name)
  self.source=self.base/'source';self.source.mkdir();self.site=self.base/'site';self.site.mkdir();(self.site/'keep.txt').write_text('keep')
  self.config=self.site/'config.inc.php';self.config.write_text('<?php return [];')
  for name in ['index.php','config.example.php','inc/service.php','inc/accounts.php','account/index.php','inc/schema.sql','inc/site.php','inc/community.php','inc/community-page.php','assets/site.css','assets/community.js','assets/blocks.js','assets/downloads.js','assets/screenshots.js','assets/screenshots/game.png','assets/screenshots/servers.png','servers/index.php','leaderboards/index.php','matches/index.php','statistics/index.php','documentation/index.php','source/index.php','credits/index.php','privacy/index.php','api/docs/index.php','inc/music-credits.json','documentation/basics/index.php','inc/basics-documentation.html','documentation/controls/index.php','inc/controls-documentation.html','documentation/features/index.php','documentation/modes/index.php','documentation/specials/index.php','documentation/server/index.php','inc/server-documentation.html','inc/source-documentation.html','inc/api-documentation.html','bin/manage.php','api/index.php','directory/index.php','.htaccess']:
   p=self.source/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text('<?php echo "test";' if p.suffix=='.php' else '# fixture')
  self.bin=self.base/'bin';self.bin.mkdir();self.record=self.base/'rsync.json'
  programs={'id': 'echo 0', 'git': 'for last do :; done; mkdir -p "$last/public-web"; cp -R "$TEST_SOURCE/." "$last/public-web/"', 'flock': 'exit 0', 'chown': 'exit 0', 'rsync': 'exec python3 -c \'import json,os,sys;open(os.environ["TEST_RECORD"],"w").write(json.dumps(sys.argv[1:]))\' "$@"'}
  for name,body in programs.items():
   p=self.bin/name;p.write_text('#!/bin/sh\n'+body+'\n');p.chmod(0o755)
  self.env=dict(os.environ,PATH=str(self.bin)+os.pathsep+os.environ['PATH'],LINECLASH_SITE=str(self.site),LINECLASH_WEB_CONFIG=str(self.config),TEST_SOURCE=str(self.source),TEST_RECORD=str(self.record))
  self.env.pop('LINECLASH_WEB_CONFIG',None)
 def run_script(self,*args):return subprocess.run(['sh',str(ROOT/'scripts/refresh-lineclash.sh'),*args],env=self.env,capture_output=True,text=True)
 def test_preview_excludes_private_files_and_does_not_load_config(self):
  self.config.write_text('<?php exit("CONFIG MUST NOT EXECUTE");')
  result=self.run_script('--dry-run');self.assertEqual(result.returncode,0,result.stderr)
  args=json.loads(self.record.read_text());self.assertIn('--dry-run',args);self.assertIn('--exclude=/config.inc.php',args);self.assertIn('--exclude=/uploads/',args)
  self.assertEqual((self.site/'keep.txt').read_text(),'keep');self.assertNotIn('CONFIG MUST NOT EXECUTE',result.stdout)
  self.assertFalse(Path(str(self.site)+'.private-backups').exists())
 def test_bad_php_stops_before_sync(self):
  (self.source/'index.php').write_text('<?php broken syntax [')
  self.assertNotEqual(self.run_script('--dry-run').returncode,0);self.assertFalse(self.record.exists())
 def test_missing_config_is_created_from_sample(self):
  self.config.unlink();sample=(self.source/'config.example.php').read_bytes()
  result=self.run_script();self.assertEqual(result.returncode,0,result.stderr)
  self.assertEqual(self.config.read_bytes(),sample);self.assertEqual(self.config.stat().st_mode&0o777,0o640)
  self.assertIn('Edit its database settings',result.stdout);self.assertFalse(Path(str(self.site)+'.private-backups').exists())
 def test_missing_config_preview_does_not_create_it(self):
  self.config.unlink();result=self.run_script('--dry-run');self.assertEqual(result.returncode,0,result.stderr)
  self.assertFalse(self.config.exists());self.assertIn('Would create',result.stdout)
 def test_missing_sample_stops_before_sync(self):
  (self.source/'config.example.php').unlink();self.config.unlink()
  self.assertNotEqual(self.run_script().returncode,0);self.assertFalse(self.record.exists());self.assertFalse(self.config.exists())
 def test_symlink_config_is_rejected(self):
  self.config.unlink();self.config.symlink_to(self.base/'missing-target')
  self.assertNotEqual(self.run_script().returncode,0);self.assertFalse(self.record.exists())
 def test_symlink_site_is_rejected(self):
  self.site=self.base/'link';self.site.symlink_to(self.base/'site',target_is_directory=True);self.env['LINECLASH_SITE']=str(self.site)
  self.assertNotEqual(self.run_script().returncode,0);self.assertFalse(self.record.exists())
 def test_initial_preview_does_not_create_site(self):
  shutil.rmtree(self.site);result=self.run_script('--dry-run');self.assertEqual(result.returncode,0,result.stderr);self.assertFalse(self.site.exists())
 def test_real_refresh_preserves_config_and_creates_private_backup(self):
  self.config.write_text('<?php return ["password"=>"local-private-value"];')
  self.config.chmod(0o640);before=self.config.stat();content=self.config.read_bytes()
  (self.source/'config.inc.php').write_text('<?php return ["password"=>"incoming-must-not-replace"];')
  result=self.run_script();self.assertEqual(result.returncode,0,result.stderr)
  self.assertEqual(self.config.read_bytes(),content);self.assertEqual(self.config.stat().st_mode,before.st_mode);self.assertEqual(self.config.stat().st_uid,before.st_uid)
  backups=Path(str(self.site)+'.private-backups');copies=list(backups.glob('config.inc.php.*'))
  self.assertEqual(len(copies),1);self.assertEqual(copies[0].read_bytes(),content)
  self.assertEqual(backups.stat().st_mode&0o777,0o700);self.assertEqual(copies[0].stat().st_mode&0o777,0o600)
  self.assertIn('--exclude=/config.inc.php',json.loads(self.record.read_text()))
 def test_default_config_path_and_ttl_without_environment(self):
  inc=self.site/'inc';inc.mkdir();shutil.copyfile(ROOT/'public-web/inc/service.php',inc/'service.php')
  self.config.write_text('<?php return ["ttl"=>240];')
  result=subprocess.run(['php','-r','require $argv[1]; echo json_encode([web_config_path(),ttl()]);',str(inc/'service.php')],env=self.env,capture_output=True,text=True)
  self.assertEqual(result.returncode,0,result.stderr);self.assertEqual(json.loads(result.stdout),[str(self.config.resolve()),240])
if __name__=='__main__':unittest.main()
