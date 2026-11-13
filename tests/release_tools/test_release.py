"""Offline release-tool acceptance tests. Real HTTP calls are forbidden."""
import contextlib
import copy
import hashlib
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tarfile
import tempfile
import unittest
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'scripts'))
import build_provenance as provenance
import release
import source_release

class FakeAPI:
    def __init__(self, service, manifest):
        self.service, self.manifest = service, manifest
        self.host = 'api.github.com' if service == 'github' else 'gitlab.test'
        self.calls, self.assets, self.packages = [], {}, {}
        self.release = None
        self.fail_upload = False
        self.wrong_digest = False
        self.base, self.path = release.release_paths(service, manifest)

    def request(self, method, path, body=None, file=None, optional=False, hash_body=False, host=None):
        self.calls.append((method, path, copy.deepcopy(body)))
        if method == 'GET' and path == self.base:
            return {'full_name': 'org/lineclash', 'permissions': {'push': True}} if self.service == 'github' else {
                'path_with_namespace': 'group/subgroup/lineclash', 'permissions': {'project_access': {'access_level': 40}}}
        if method == 'GET' and path == self.path:
            return copy.deepcopy(self.release)
        if '/packages/generic/' in path:
            if method == 'GET':
                return self.packages.get(path)
            if self.fail_upload:
                raise RuntimeError('simulated upload interruption')
            self.packages[path] = release.digest(file)
            return {'message': '201 Created'}
        if method == 'POST' and path == self.base + '/releases':
            self.release = {'id': 7, 'tag_name': body['tag_name'], 'body': body.get('body'),
                            'description': body.get('description'), 'draft': body.get('draft', False),
                            'upload_url': 'https://uploads.github.com' + self.base + '/releases/7/assets{?name,label}',
                            'assets': body.get('assets', {'links': []})}
            return copy.deepcopy(self.release)
        if method == 'GET' and '/assets?per_page=' in path:
            return list(self.assets.values())
        if method == 'POST' and '/assets?name=' in path:
            if self.fail_upload:
                raise RuntimeError('simulated upload interruption')
            name = file.name
            value = {'name': name, 'digest': 'sha256:' + ('0' * 64 if self.wrong_digest else release.digest(file))}
            self.assets[name] = value
            return value
        if method == 'PATCH':
            self.release.update(body)
            return copy.deepcopy(self.release)
        if method == 'POST' and path.endswith('/assets/links'):
            self.release['assets']['links'].append(copy.deepcopy(body))
            return body
        raise AssertionError((method, path))



class ReleaseTests(unittest.TestCase):
    def setUp(self):
        self.addCleanup(patch.stopall)
        patch('platform_packages.preflight',return_value='').start()
        patch('platform_packages.installers',return_value=[]).start()
        patch('platform_packages.verify_installers').start()
        patch('windows_runtime.runtime_files',return_value=[]).start()
        self.tmp=tempfile.TemporaryDirectory(); self.addCleanup(self.tmp.cleanup)
        self.root=Path(self.tmp.name)
        files={'version.mk':'VERSION ?= 1.0.0\n','get-version.sh':'#!/bin/sh\n',
               'Makefile':'all:\n','LICENSE.md':'license fixture','README.md':'readme fixture',
               'docs/RELEASE-NOTES.md':'Release notes','server/lineclashd.cfg.example':'port=31457\n',
               'clients/gui/src/client.c':'source','clients/cli/src/client.c':'source',
               'server/src/server.c':'source','shared/src/net.c':'source','tests/test.c':'test',
               'scripts/test.py':'# fixture','public-web/index.php':'<?php echo "test";',
               '.gitignore':'outputs/\nserver/lineclashd\nserver/lineclashd-tool\nclients/gui/lineclash\nclients/cli/lineclash-cli\n*.exe\nbuild/\n*.o\n__pycache__/\n'}
        for name,data in files.items():
            p=self.root/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text(data)
        for args in (['git','init','-q'],['git','config','user.name','Release Test'],
                     ['git','config','user.email','test@example.invalid'],['git','add','.'],
                     ['git','commit','-qm','fixture']):
            subprocess.run(args,cwd=self.root,check=True,capture_output=True)
        for path,name in release.native_files(self.root):
            p=self.root/path
            if not p.exists():p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(b'binary fixture')
        self.config={'github':'https://github.com/org/lineclash','github_git':'git@github.com:org/lineclash.git',
                     'gitlab':'https://gitlab.test/group/subgroup/lineclash','gitlab_git':'git@gitlab.test:group/subgroup/lineclash.git',
                     'source_branch':'main','notary_profile':'','env':{},'credentials':{}}
        release._ACTIVE_SECRETS.clear()
        self.addCleanup(patch.stopall)
        patch('http.client.HTTPSConnection',side_effect=AssertionError('Real networking forbidden')).start()
        patch.dict(os.environ,{'GITHUB_TOKEN':'mock-github-token','GITLAB_TOKEN':'mock-gitlab-token'}).start()
        with patch('release.run') as run:release.build(self.config,self.root)

    def manifest(self):
        with contextlib.redirect_stdout(io.StringIO()):
            return release.prepare(self.config,self.root/'docs/RELEASE-NOTES.md',self.root)

    def services(self):
        manifest=self.manifest(); apis={s:FakeAPI(s,manifest) for s in release.SERVICES}; remotes={}
        patch('release.API',side_effect=lambda host,token,github=False:apis['github' if github else 'gitlab']).start()
        patch('release.remote_tag',side_effect=lambda url,tag,root:remotes.get(url)).start()
        def run(args,root,capture=False,env=None):
            if args[:2]==['git','push']:
                remotes[args[3]]=manifest['commit'];return None
            return provenance.run(args,root,capture,env)
        patch('release.run',side_effect=run).start()
        return manifest,apis,remotes

    def test_linux_native_archive_is_tar_with_executable_modes(self):
        import shutil
        shutil.rmtree(self.root/'outputs')
        with patch('release.platform.system',return_value='Linux'),patch('release.run'):
            release.build(self.config,self.root)
            record=release.read_json(release.output(self.root)/'BUILD-PROVENANCE.json')
            self.assertTrue(record['native_asset'].endswith('.tar.gz'))
            with tarfile.open(release.output(self.root)/record['native_asset']) as archive:
                self.assertEqual(archive.getmember('bin/lineclashd').mode,0o755)
            release.check(self.root)

    def test_amiga_executable_is_not_source_input(self):
        folder=self.root/'clients/amiga';folder.mkdir()
        (folder/'LineClash').write_bytes(bytes.fromhex('000003f3'))
        (folder/'main.c').write_text('int main(void){return 0;}')
        inputs={str(path.relative_to(self.root)) for path in provenance.source_files(self.root)}
        self.assertNotIn('clients/amiga/LineClash',inputs)
        self.assertIn('clients/amiga/main.c',inputs)

    def test_inventory_valid(self):self.assertIn('SHA256SUMS',release.check(self.root))
    def test_plan_does_not_build_or_contact_network(self):
        with patch('release.run',side_effect=AssertionError('must not run')):
            self.assertEqual(release.build_commands(self.config),[['bash','scripts/verify-platform.sh']])
    def test_source_change_rejected(self):
        (self.root/'README.md').write_text('changed')
        with self.assertRaises(ValueError):release.check(self.root)
    def test_artifact_change_rejected(self):
        record=provenance.read_json(release.output(self.root)/release.names(self.root)['record'])
        (release.output(self.root)/record['native_asset']).write_bytes(b'changed')
        with self.assertRaises(ValueError):release.check(self.root)
    def test_extra_inventory_rejected(self):
        (release.output(self.root)/'unexpected').write_text('extra')
        with self.assertRaises(ValueError):release.check(self.root)
    def test_symlink_asset_rejected(self):
        p=release.output(self.root)/release.names(self.root)['source'];p.unlink();p.symlink_to(self.root/'README.md')
        with self.assertRaises(ValueError):release.check(self.root)
    def test_source_archive_deterministic_and_excludes_outputs(self):
        a=self.root/'a.tar.gz';b=self.root/'b.tar.gz';source_release.create(a,self.root);source_release.create(b,self.root)
        self.assertEqual(a.read_bytes(),b.read_bytes())
        with tarfile.open(a) as archive:
            names=archive.getnames()
            self.assertFalse(any('/outputs/' in name or name.endswith('/bin/lineclash') for name in names))
    def test_private_web_config_and_delivery_state_excluded(self):
        private=['public-web/config.php','public-web/config.inc.php','public-web/secrets.local.php','public-web/.env','server/directory.sqlite3','server/directory.sqlite3-wal','server/directory.sqlite3-shm','server/directory.sqlite3.health.json']
        for name in private:
            (self.root/name).write_text('PRIVATE TEST VALUE')
        names={str(p.relative_to(self.root)) for p in provenance.source_files(self.root)}
        self.assertFalse(names.intersection(private))
        self.assertFalse(any(p.name in {'config.php','config.inc.php','.env','secrets.local.php'} for p in release.website_files(self.root)))

    def test_unknown_url_or_mismatched_push_destination_rejected(self):
        for url in ['http://github.com/org/lineclash','https://secret@github.com/org/lineclash','https://github.com/org/../lineclash']:
            with self.assertRaises(ValueError):release.repository(url,True)
        cfg=dict(self.config,github_git='git@github.com:other/lineclash.git')
        with self.assertRaises(ValueError):release.git_destination(cfg,'github')
    def test_publish_requires_explicit_confirmation(self):
        self.manifest()
        with self.assertRaises(ValueError):release.publish(['gitlab'],root=self.root,config=self.config)
    def test_staged_tampering_rejected(self):
        manifest=self.manifest();p=release.publication_folder(self.root)/next(iter(manifest['files']))
        p.write_bytes(b'changed')
        with self.assertRaises(ValueError):release.verify_publication(manifest,self.root)
    def test_notes_and_tag_are_frozen(self):
        manifest=release.prepare(self.config,self.root/'docs/RELEASE-NOTES.md',self.root,tag='v1.0.0-test',tag_message='LineClash test')
        self.assertEqual(manifest['tag'],'v1.0.0-test');self.assertEqual(manifest['tag_message'],'LineClash test')
    def test_missing_token_stops_before_tag_mutation(self):
        _,apis,_=self.services()
        with patch('release.credential',side_effect=ValueError('missing token')):
            with self.assertRaises(ValueError):release.publish(['gitlab','github'],True,self.root,self.config)
        self.assertFalse(provenance.run(['git','tag','--list'],self.root,True))
        self.assertFalse(any(method!='GET' for api in apis.values() for method,_,_ in api.calls))
    def test_conflicting_remote_tag_stops_all_writes(self):
        _,apis,remotes=self.services();remotes[self.config['github_git']]='f'*40
        with self.assertRaises(ValueError):release.publish(['gitlab','github'],True,self.root,self.config)
        self.assertFalse(any(method!='GET' for api in apis.values() for method,_,_ in api.calls))
    def test_both_services_publish_and_resume_exact_assets(self):
        manifest,apis,_=self.services();release.publish(['gitlab','github'],True,self.root,self.config)
        self.assertFalse(apis['github'].release['draft'])
        self.assertEqual(set(apis['github'].assets),set(manifest['files']))
        self.assertEqual(set(apis['gitlab'].packages),set(release.package_path(apis['gitlab'].base,manifest,n) for n in manifest['files']))
        uploads=sum(method in ('POST','PUT') for a in apis.values() for method,_,_ in a.calls)
        release.publish(['gitlab','github'],True,self.root,self.config)
        self.assertEqual(uploads,sum(method in ('POST','PUT') for a in apis.values() for method,_,_ in a.calls))
    def test_wrong_github_digest_leaves_draft(self):
        _,apis,_=self.services();apis['github'].wrong_digest=True
        with self.assertRaises(ValueError):release.publish(['github'],True,self.root,self.config)
        self.assertTrue(apis['github'].release['draft'])
    def test_gitlab_download_mismatch_is_not_overwritten(self):
        manifest,apis,_=self.services();name=next(iter(manifest['files']))
        apis['gitlab'].packages[release.package_path(apis['gitlab'].base,manifest,name)]='0'*64
        with self.assertRaises(ValueError):release.publish(['gitlab'],True,self.root,self.config)
        self.assertFalse(any(m in ('PUT','POST') for m,_,_ in apis['gitlab'].calls))
    def test_archive_tool_does_not_read_credentials_during_build(self):
        with patch('release.credential',side_effect=AssertionError('must not resolve credentials')):
            release.check(self.root)
    def test_api_redirects_cannot_receive_credentials(self):
        api=release.API('api.github.com','not-a-real-token',True)
        with self.assertRaises(ValueError):api.request('GET','/path',host='evil.test')

if __name__=='__main__':unittest.main()
