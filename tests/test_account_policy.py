#!/usr/bin/env python3
"""Configurable central password policy, including Unicode and invalid settings."""
import json,os,shutil,subprocess,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
@unittest.skipUnless(shutil.which('php'),'PHP CLI is required')
class PolicyTests(unittest.TestCase):
 def policy(self,password,setting=None):
  with tempfile.TemporaryDirectory() as d:
   config=Path(d)/'config.php';config.write_text('<?php return '+('[]' if setting is None else "['account_password_min_length'=>"+json.dumps(setting)+']')+';')
   code='require $argv[1]; $a=json_decode(stream_get_contents(STDIN),true); try {account_password($a["password"]); echo "accepted";} catch(InvalidArgumentException $e) {echo $e->getMessage();}'
   return subprocess.check_output(['php','-r',code,str(ROOT/'public-web/inc/accounts.php')],input=json.dumps(dict(password=password)).encode(),env=dict(os.environ,LINECLASH_WEB_CONFIG=str(config))).decode()
 def test_default_requires_twelve_characters(self):self.assertIn('at least 12',self.policy('x'*11));self.assertEqual('accepted',self.policy('x'*12))
 def test_configurable_shorter_minimum(self):self.assertEqual('accepted',self.policy('x'*6,6));self.assertIn('at least 6',self.policy('x'*5,6))
 def test_minimum_can_be_one(self):self.assertEqual('accepted',self.policy('x',1));self.assertIn('at least 1 character.',self.policy('',1))
 def test_multibyte_letters_count_as_characters(self):self.assertIn('at least 12',self.policy('é'*11));self.assertEqual('accepted',self.policy('é'*12))
 def test_byte_limit_stays_enforced(self):self.assertIn('too long',self.policy('é'*37,1));self.assertEqual('accepted',self.policy('x'*72,1))
 def test_bad_settings_keep_default(self):
  for value in (0,73,'6'):
   with self.subTest(value=value):self.assertIn('at least 12',self.policy('x'*11,value))
if __name__=='__main__':unittest.main()
