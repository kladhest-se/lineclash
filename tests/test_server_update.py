#!/usr/bin/env python3
"""Exercise the Linux updater without root, systemd or a production server."""
import os
from pathlib import Path
import sqlite3
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

class ServerUpdate(unittest.TestCase):
    def test_install_update_and_failure(self):
        with tempfile.TemporaryDirectory(prefix='lineclash-updater-') as directory:
            base=Path(directory).resolve();repo=base/'upstream';repo.mkdir()
            for name in ('server','shared','scripts'):(repo/name).mkdir()
            (repo/'server/lineclashd.cfg.example').write_text('port = 31457\ndb_file = database.sqlite3\nlog_file = lineclashd.log\n')
            (repo/'shared/fixture.c').write_text('fixture\n')
            (repo/'scripts/server_directory.py').write_text('# fixture\n')
            for name in ('version.mk','get-version.sh','LICENSE.md'):(repo/name).write_text('fixture\n')
            def git(*args):
                return subprocess.run(['git','-C',str(repo),*args],check=True,capture_output=True,text=True)
            git('init','-b','main');git('add','.')
            git('-c','user.name=Fixture','-c','user.email=fixture@example.invalid','commit','-m','fixture')
            commands=base/'commands';commands.mkdir()
            dispatcher=commands/'dispatch'
            dispatcher.write_text('''#!/usr/bin/env python3
import os,sys,subprocess
from pathlib import Path
name=Path(sys.argv[0]).name;args=sys.argv[1:];base=Path(os.environ['FIXTURE'])
if name=='id':print(0)
elif name=='uname':print('Linux')
elif name in ('chown','chgrp','flock','sleep','useradd'):pass
elif name=='getent':print('lineclash:x:999:999::/var/lib/lineclashd:/usr/sbin/nologin')
elif name=='install':
    clean=[];i=0
    while i<len(args):
        if args[i] in ('-o','-g'):i+=2
        else:clean.append(args[i]);i+=1
    sys.exit(subprocess.call(['/usr/bin/install',*clean]))
elif name=='make':
    if os.environ.get('FAIL_BUILD'):sys.exit(1)
    target=Path(args[args.index('-C')+1])
    for item in ('lineclashd','lineclashd-tool'):
        path=target/item;path.write_text('#!/bin/sh\\necho fixture-version\\n');path.chmod(0o755)
elif name=='systemctl':
    with (base/'service-commands').open('a') as f:f.write(' '.join(args)+'\\n')
    state=base/'active'
    if args[0]=='is-active':sys.exit(0 if state.exists() else 3)
    elif args[0]=='stop':
        if not state.exists():sys.exit(5)
        state.unlink()
    elif args[0]=='restart' and not os.environ.get('FAIL_START'):state.touch()
elif name=='journalctl':print('fixture startup failed')
else:sys.exit('Unexpected fixture command: '+name)
''')
            dispatcher.chmod(0o755)
            for name in ('id','uname','chown','chgrp','flock','sleep','getent','install','make','systemctl','journalctl','useradd'):(commands/name).symlink_to(dispatcher)
            env={**os.environ,'PATH':str(commands)+':'+os.environ['PATH'],'FIXTURE':str(base),
                'LINECLASH_SERVER_REPOSITORY':str(repo),'LINECLASH_SERVER_SOURCE':str(base/'source'),
                'LINECLASH_SERVER_PREFIX':str(base/'installed'),'LINECLASH_SERVER_CONFIG':str(base/'server.cfg'),
                'LINECLASH_SERVER_STATE':str(base/'state'),'LINECLASH_SYSTEMD_DIR':str(base/'units'),
                'LINECLASH_UPDATE_LOCK':str(base/'update.lock')}
            def run(**extra):return subprocess.run(['bash',str(ROOT/'scripts/update-server.sh')],env={**env,**extra},capture_output=True,text=True)
            first=run();self.assertEqual(first.returncode,0,first.stdout+first.stderr)
            self.assertTrue((base/'active').exists())
            unit=(base/'units/lineclashd.service').read_text()
            self.assertIn('User=lineclash',unit);self.assertIn('StandardInput=null',unit)
            self.assertIn(str(base/'server.cfg'),unit)
            config=base/'server.cfg';original=config.read_bytes()
            key=base/'state/database.sqlite3.directory-key';key.write_text('keep-directory-key')
            db=base/'state/database.sqlite3'
            with sqlite3.connect(db) as connection:connection.execute('CREATE TABLE preserve (value)');connection.execute("INSERT INTO preserve VALUES ('keep-account')")
            updated=run();self.assertEqual(updated.returncode,0,updated.stdout+updated.stderr)
            self.assertEqual(config.read_bytes(),original);self.assertEqual(key.read_text(),'keep-directory-key')
            with sqlite3.connect(db) as connection:self.assertEqual(connection.execute('SELECT value FROM preserve').fetchone()[0],'keep-account')
            backups=list((base/'installed/backups').glob('*/database.sqlite3'));self.assertTrue(backups)
            with sqlite3.connect(backups[-1]) as connection:self.assertEqual(connection.execute('SELECT value FROM preserve').fetchone()[0],'keep-account')
            calls=(base/'service-commands').read_bytes()
            failed=run(FAIL_BUILD='1');self.assertNotEqual(failed.returncode,0)
            self.assertEqual((base/'service-commands').read_bytes(),calls);self.assertTrue((base/'active').exists())
            (base/'source/version.mk').write_text('local change\n')
            dirty=run();self.assertNotEqual(dirty.returncode,0);self.assertIn('local source edits',dirty.stderr)
            self.assertEqual((base/'service-commands').read_bytes(),calls)

if __name__=='__main__':unittest.main()
