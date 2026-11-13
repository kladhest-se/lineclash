#!/usr/bin/env python3
"""Build once, prepare an exact manifest, then explicitly publish verified bytes."""
import argparse
import hashlib
import http.client
import json
import os
import platform
import io
import gzip
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tarfile
from urllib.parse import quote, urlsplit
from zipfile import ZipFile

from build_provenance import (ROOT, commit, digest, read_json, require, run,
                              source_files, source_hash, version, write_json)
from source_release import create as create_source
from release_support import (credential, default_config_path, push_source,
                             release_tag, validate_credentials)

SERVICES = ('gitlab', 'github')
CONFIG_KEYS = {'github', 'github_git', 'gitlab', 'gitlab_git', 'notary_profile', 'env', 'credentials', 'source_branch'}
ENV_KEYS = {'SIGN_IDENTITY', 'PHP', 'LINECLASH_RELEASE_PYTHON'}
_ACTIVE_SECRETS = set()


def secret_check(data):
    if isinstance(data, str):
        data = data.encode()
    for token in _ACTIVE_SECRETS | {os.environ.get(name, '') for name in ('GITHUB_TOKEN', 'GITLAB_TOKEN', 'LINECLASH_DIRECTORY_TOKEN')}:
        require(not token or len(token) < 8 or token.encode() not in data,
                'A credential occurs in release inputs; remove it before proceeding')


def configuration(path):
    path = Path(path).expanduser()
    config = {'github': '', 'github_git': '', 'gitlab': '', 'gitlab_git': '',
              'notary_profile': '', 'env': {}, 'credentials': {}, 'source_branch': ''}
    if path.exists():
        supplied = read_json(path)
        require(isinstance(supplied, dict) and not set(supplied) - CONFIG_KEYS,
                'Unknown release configuration keys')
        config.update(supplied)
    require(isinstance(config['env'], dict) and not set(config['env']) - ENV_KEYS,
            'Only SIGN_IDENTITY, PHP and LINECLASH_RELEASE_PYTHON are allowed in configuration env')
    require(all(isinstance(v, str) and '\n' not in v and '\0' not in v
                for k, v in config.items() if k not in ('env', 'credentials')), 'Invalid configuration value')
    require(all(isinstance(v, str) and '\0' not in v and '\n' not in v
                for v in config['env'].values()), 'Invalid build environment')
    validate_credentials(config['credentials'])
    if config['source_branch']:
        run(['git', 'check-ref-format', '--branch', config['source_branch']], ROOT, True)
    secret_check(json.dumps(config))
    for service in SERVICES:
        if config[service]:
            git_destination(config, service)
    return config


def repository(url, github=False):
    parsed = urlsplit(url)
    path = parsed.path.strip('/')
    if path.endswith('.git'):
        path = path[:-4]
    require(parsed.scheme == 'https' and parsed.hostname and not parsed.username
            and not parsed.password and not parsed.query and not parsed.fragment
            and not parsed.port and not any(ord(c) < 33 for c in url)
            and re.fullmatch(r'[A-Za-z0-9_.-]+(?:/[A-Za-z0-9_.-]+)+', path),
            'Use a credential-free HTTPS repository URL')
    require(all(p not in ('.', '..') for p in path.split('/')), 'Invalid repository path')
    require(not github or (parsed.hostname == 'github.com' and len(path.split('/')) == 2),
            'GitHub destination must be https://github.com/OWNER/REPO')
    return parsed.hostname, path


def git_destination(config, service):
    host, project = repository(config[service], service == 'github')
    destination = config.get(service + '_git') or 'https://' + host + '/' + project + '.git'
    allowed = {'git@' + host + ':' + project + '.git',
               'https://' + host + '/' + project, 'https://' + host + '/' + project + '.git'}
    require(destination in allowed, 'Git push destination must match the release repository')
    return destination


def names(root=ROOT):
    v = version(root)
    return {'source': 'lineclash-' + v + '-source.tar.gz',
            'website': 'lineclash-' + v + '-website.tar.gz', 'record': 'BUILD-PROVENANCE.json'}


def output(root=ROOT):
    folder = root / 'outputs'
    require(not folder.is_symlink() and not (folder / 'artifacts').is_symlink(), 'Symlinked output directory')
    return folder / 'artifacts'


def inventory(root=ROOT):
    folder = output(root); n = names(root)
    record = read_json(folder / n['record'])
    assets = record.get('artifacts', {})
    require(isinstance(assets, dict) and assets and n['website'] in assets, 'Missing artifact record')
    require(all(re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_.-]*', name) for name in assets), 'Invalid artifact name')
    require(record.get('native_asset') in assets and record['native_asset'].endswith(('.zip','.tar.gz')), 'Missing native archive')
    required = set(n.values()) | set(assets)
    sums = folder / 'SHA256SUMS'
    require(folder.is_dir() and not sums.is_symlink(), 'Build release artifacts first')
    require({p.name for p in folder.iterdir()} == required | {'SHA256SUMS'}, 'Missing or unexpected release artifact inventory')
    files = {}
    for line in sums.read_text().splitlines():
        match = re.fullmatch(r'([a-f0-9]{64})  ([A-Za-z0-9][A-Za-z0-9_.-]*)', line)
        require(match is not None and match[2] not in files, 'Invalid or duplicate checksum entry')
        files[match[2]] = match[1]
    require(set(files) == required, 'Checksum inventory differs from required artifacts')
    for name, sha in files.items():
        require(digest(folder / name) == sha, 'Missing or changed asset: ' + name)
    files['SHA256SUMS'] = digest(sums)
    return files


def website_files(root=ROOT):
    return [p for p in source_files(root) if p.is_relative_to(root / 'public-web')]


def write_tar(destination, entries):
    require(not destination.is_symlink(), 'Symlinked archive')
    with destination.open('wb') as stream, gzip.GzipFile(fileobj=stream, mode='wb', mtime=0, filename='') as gz:
        with tarfile.open(fileobj=gz, mode='w') as archive:
            for name, data, mode in entries:
                info=tarfile.TarInfo(name); info.size=len(data); info.mode=mode; info.mtime=0
                archive.addfile(info,io.BytesIO(data))


def native_files(root=ROOT, windows=None, runtime_dirs=(), objdump=None):
    windows = platform.system() == 'Windows' if windows is None else windows
    suffix = '.exe' if windows else ''
    entries = [('server/lineclashd'+suffix, 'bin/lineclashd'+suffix),
               ('server/lineclashd-tool'+suffix, 'bin/lineclashd-tool'+suffix),
               ('clients/gui/lineclash'+suffix, 'bin/lineclash'+suffix)]
    if windows:
        from windows_runtime import runtime_files
        executables=[root/path for path,name in entries]
        for dll in runtime_files(executables,[p.parent for p in executables]+list(runtime_dirs),objdump):
            entries.append((str(dll),'bin/'+dll.name))
    entries += [('LICENSE.md','LICENSE.md'),('README.md','README.md'),
                ('server/lineclashd.windows.cfg.example' if windows else 'server/lineclashd.cfg.example','lineclashd.cfg.example')]
    if (root/'scripts/server_directory.py').is_file():
        entries.append(('scripts/server_directory.py','scripts/server_directory.py'))
    if (root/'scripts/refresh-lineclash.sh').is_file():
        entries.append(('scripts/refresh-lineclash.sh','scripts/refresh-lineclash.sh'))
    if (root/'scripts/verify_replay.py').is_file():
        entries.append(('scripts/verify_replay.py','scripts/verify_replay.py'))
    if (root/'CREDITS.md').is_file(): entries.append(('CREDITS.md','CREDITS.md'))
    entries += [(str(p.relative_to(root)),str(p.relative_to(root))) for p in sorted((root/'docs').rglob('*')) if p.is_file()]
    entries += [(str(p.relative_to(root)),str(p.relative_to(root))) for p in source_files(root)
                if p.relative_to(root).parts[0]=='music' and not p.name.startswith('.')]

    return entries


def check(root=ROOT):
    files = inventory(root); n = names(root)
    record = read_json(output(root)/n['record'])
    require(record.get('source_sha256') == source_hash(root), 'Source changed; rebuild release artifacts')
    require(record.get('version') == version(root), 'Build version differs from version.mk')
    require(record.get('commit') == commit(root), 'Build commit changed; rebuild release artifacts')
    require(record['artifacts'] == {name:files[name] for name in record['artifacts']}, 'Build record differs from artifact hashes')
    expected={'lineclash-'+version(root)+'/'+str(p.relative_to(root)):p.read_bytes() for p in source_files(root)}
    expected['lineclash-'+version(root)+'/BUILD-PROVENANCE.json']=(output(root)/n['record']).read_bytes()
    for filename, contents in [(n['source'],expected),
        (n['website'],{str(p.relative_to(root/'public-web')):p.read_bytes() for p in website_files(root)})]:
        with tarfile.open(output(root)/filename) as archive:
            actual={}
            for item in archive.getmembers():
                require(item.isfile() and item.name not in actual, 'Unexpected archive entry')
                actual[item.name]=archive.extractfile(item).read()
            require(actual==contents,'Archive differs from source: '+filename)
    # Imported multi-platform assets are bound by digest. The native ZIP is
    # additionally compared to the exact binaries/docs built on this host.
    require(record.get('platform')==platform.system() and record.get('architecture')==platform.machine(),
            'Validate/publish native build outputs on the host platform recorded by build')
    expected=record.get('native_entries') if record['platform']=='Darwin' else {name:digest(root/path) for path,name in native_files(root)}
    require(isinstance(expected,dict) and expected,'Missing native payload inventory')
    actual={}
    if record['native_asset'].endswith('.tar.gz'):
        with tarfile.open(output(root)/record['native_asset']) as archive:
            for item in archive.getmembers():
                require(item.isfile() and item.name not in actual,'Unexpected native tar entry')
                actual[item.name]=hashlib.sha256(archive.extractfile(item).read()).hexdigest()
    else:
        with ZipFile(output(root)/record['native_asset']) as archive:
            for item in archive.infolist():
                if record['platform']=='Darwin' and item.is_dir():continue
                require(not item.is_dir() and item.filename not in actual,'Unexpected native ZIP entry')
                actual[item.filename]=hashlib.sha256(archive.read(item)).hexdigest()
    require(actual==expected,'Native archive differs from the verified build')
    from platform_packages import verify_installers
    verify_installers(record,output(root),root)
    return files


def build_commands(config=None):
    return [['bash','scripts/verify-platform.sh']]


def build(config, root=ROOT, assets=()):
    require(not (root/'outputs/release/receipt.json').exists(),'Preserve existing release evidence before rebuilding')
    folder=output(root)
    require(not folder.exists() or not any(folder.iterdir()),'Move old outputs/artifacts aside before rebuilding')
    before, revision=source_hash(root),commit(root)
    for path in source_files(root): secret_check(path.read_bytes())
    env=dict(os.environ,**config['env'])
    for name in ('GITHUB_TOKEN','GITLAB_TOKEN','LINECLASH_DIRECTORY_TOKEN','VERSION'):env.pop(name,None)
    for spec in config.get('credentials',{}).values():
        if spec.get('provider','env')=='env':env.pop(spec.get('variable',''),None)
    env['PYTHON']=env.get('LINECLASH_RELEASE_PYTHON',sys.executable)
    from platform_packages import preflight, sign_binaries, installers
    identity=preflight(env)
    for command in build_commands(config):run(command,root,env=env)
    if identity:sign_binaries(root,identity)
    require(before==source_hash(root) and revision==commit(root),'Source changed during build')
    folder.mkdir(parents=True,exist_ok=True); n=names(root)
    system=platform.system()
    native='LineClash-'+version(root)+'-'+('macos' if system=='Darwin' else system.lower())+'-'+platform.machine()+('.tar.gz' if system=='Linux' else '.zip')
    if system=='Linux':
        write_tar(folder/native,[(name,(root/path).read_bytes(),0o755 if name.startswith('bin/') else 0o644) for path,name in native_files(root)])
    else:
        with ZipFile(folder/native,'w',compression=8) as archive:
            for path,name in native_files(root):
                digest(root/path); archive.write(root/path,name)
    extra=installers(root,folder,version(root),identity,env)
    write_tar(folder/n['website'],[(str(p.relative_to(root/'public-web')),p.read_bytes(),0o644) for p in website_files(root)])
    native_entries={}
    if system=='Darwin':
        with ZipFile(folder/native) as archive:
            native_entries={item.filename:hashlib.sha256(archive.read(item)).hexdigest() for item in archive.infolist() if not item.is_dir()}
    recorded={native:digest(folder/native),n['website']:digest(folder/n['website'])}
    recorded.update({p.name:digest(p) for p in extra})
    for item in assets:
        path=Path(item).expanduser().resolve()
        require(re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_.-]*',path.name) and path.name not in set(n.values())|set(recorded)|{'SHA256SUMS'},'Duplicate/invalid extra asset')
        sha=digest(path); secret_check(path.read_bytes()); shutil.copyfile(path,folder/path.name)
        recorded[path.name]=sha
    record={'version':version(root),'commit':revision,'source_sha256':before,
            'platform':platform.system(),'architecture':platform.machine(),'native_asset':native,'installer_assets':[p.name for p in extra],
            'signing_identity':identity or '', 'native_entries':native_entries,
            'runtime_validation':'Native automated checks passed; other OS runtime and installer checks require their own hosts.',
            'dependencies':('Windows non-system runtime DLLs are bundled; extract the complete ZIP.' if platform.system()=='Windows' else 'Native binaries require SDL2 and libcurl (GUI on POSIX), and SQLite runtime; ZIP is not a standalone installer.'),
            'artifacts':recorded}
    write_json(folder/n['record'],record)
    create_source(folder/n['source'],root,(folder/n['record']).read_bytes())
    require(before==source_hash(root),'Source changed during packaging')
    (folder/'SHA256SUMS').write_text(''.join(digest(p)+'  '+p.name+'\n' for p in sorted(folder.iterdir()) if p.name!='SHA256SUMS'))
    check(root)


def publication_assets(root=ROOT):
    files=inventory(root); sidecars={}
    for name,sha in list(files.items()):
        if name=='SHA256SUMS':continue
        data=(sha+'  '+name+'\n').encode()
        files[name+'.sha256']=hashlib.sha256(data).hexdigest(); sidecars[name+'.sha256']=data
    return files,sidecars


def publication_folder(root=ROOT):
    folder = root / 'outputs/release/publish'
    require(not folder.is_symlink() and not folder.parent.is_symlink(), 'Symlinked publication stage')
    return folder


def verify_publication(manifest, root=ROOT):
    expected, _ = publication_assets(root)
    require(manifest.get('files') == expected, 'Prepared publication inventory differs')
    folder = publication_folder(root)
    require(folder.is_dir() and {p.name for p in folder.iterdir()} == set(expected), 'Unexpected staged assets')
    for name, sha in expected.items():
        require(digest(folder / name) == sha, 'Staged publication asset changed: ' + name)
    return folder


def prepare(config, notes, root=ROOT, tag=None, tag_message=None):
    inventory_files = check(root)
    files, sidecars = publication_assets(root)
    tag = release_tag(tag, version(root))
    annotation = tag_message if tag_message is not None else 'LineClash ' + tag
    require(annotation.strip() and '\0' not in annotation, 'Invalid tag annotation')
    for source in source_files(root): secret_check(source.read_bytes())
    text = notes.read_text().strip()
    require(text, 'Release notes cannot be empty')
    secret_check(text)
    manifest = {'version': version(root), 'tag': tag, 'tag_message': annotation,
                'commit': commit(root), 'source_sha256': source_hash(root), 'notes': text,
                'files': files, 'inventory': inventory_files}
    for service in SERVICES:
        manifest[service] = config[service]
        manifest[service + '_git'] = git_destination(config, service) if config[service] else ''
    path = root / 'outputs/release/manifest.json'
    # An identical prepare is harmless; changed manifests never reuse a publication receipt.
    receipt = path.parent / 'receipt.json'
    if receipt.exists():
        require(path.exists() and read_json(path) == manifest,
                'A receipt exists for another manifest; preserve its release evidence first')
    folder = publication_folder(root)
    folder.mkdir(parents=True, exist_ok=True)
    require(not set(p.name for p in folder.iterdir()) - set(files), 'Unexpected staged assets; review before preparing')
    for name in files:
        require(not (folder / name).is_symlink(), 'Symlinked staged asset')
        if name.endswith('.sha256'):
            (folder / name).write_bytes(sidecars[name])
        else:
            shutil.copyfile(output(root) / name, folder / name)
    verify_publication(manifest, root)
    secret_check(json.dumps(manifest))
    write_json(path, manifest)
    print('Prepared outputs/release/manifest.json; review notes, destinations and hashes.')
    return manifest


class API:
    """HTTPS streaming transfers; no redirects, response-body logging or automatic retry."""
    def __init__(self, host, token, github=False):
        self.host, self.token, self.github = host, token, github

    def request(self, method, path, body=None, file=None, optional=False, hash_body=False, host=None):
        target = host or self.host
        require(target == self.host or (self.github and target == 'uploads.github.com'), 'Unexpected API host')
        require(path.startswith('/') and not path.startswith('//'), 'Invalid API path')
        headers = {'User-Agent': 'LineClash-release', 'Accept': 'application/json'}
        if self.github:
            headers.update({'Authorization': 'Bearer ' + self.token, 'X-GitHub-Api-Version': '2022-11-28'})
        else:
            headers['PRIVATE-TOKEN'] = self.token
        data = json.dumps(body).encode() if body is not None else None
        if data is not None:
            headers['Content-Type'] = 'application/json'
        connection = http.client.HTTPSConnection(target, timeout=120)
        stream = None
        try:
            if file is not None:
                stream = file.open('rb')
                data = stream
                headers.update({'Content-Type': 'application/octet-stream', 'Content-Length': str(file.stat().st_size)})
            connection.request(method, path, body=data, headers=headers)
            response = connection.getresponse()
            if optional and response.status == 404:
                response.read()
                return None
            require(200 <= response.status < 300,
                    'Release API returned HTTP ' + str(response.status) + '; no automatic retry')
            if hash_body:
                h = hashlib.sha256()
                for chunk in iter(lambda: response.read(1024 * 1024), b''):
                    h.update(chunk)
                return h.hexdigest()
            payload = response.read()
            return json.loads(payload) if payload else {}
        finally:
            if stream:
                stream.close()
            connection.close()


def remote_tag(url, tag, root=ROOT):
    text = run(['git', 'ls-remote', '--tags', url, 'refs/tags/' + tag, 'refs/tags/' + tag + '^{}'], root, True)
    refs = dict((ref, sha) for sha, ref in (line.split() for line in text.splitlines()))
    return refs.get('refs/tags/' + tag + '^{}', refs.get('refs/tags/' + tag))


def release_paths(service, manifest):
    _, project = repository(manifest[service], service == 'github')
    base = '/repos/' + project if service == 'github' else '/api/v4/projects/' + quote(project, safe='')
    path = base + '/releases/' + ('tags/' if service == 'github' else '') + quote(manifest['tag'], safe='')
    return base, path


def github_assets(api, base, release):
    assets, page = {}, 1
    while True:
        entries = api.request('GET', base + '/releases/' + str(release['id']) + '/assets?per_page=100&page=' + str(page))
        for item in entries:
            require(item['name'] not in assets, 'Duplicate GitHub asset names')
            assets[item['name']] = item
        if len(entries) < 100:
            return assets
        page += 1


def package_path(base, manifest, name):
    return base + '/packages/generic/lineclash/' + quote(manifest['version'], safe='') + '/' + quote(name, safe='')


def inspect_release(service, manifest, api):
    base, path = release_paths(service, manifest)
    release = api.request('GET', path, optional=True)
    if release is not None:
        field = 'body' if service == 'github' else 'description'
        require((release.get(field) or '') == manifest['notes'], service + ': existing release notes differ')
        require(release.get('tag_name') == manifest['tag'], service + ': release tag differs')
    if service == 'github':
        assets = github_assets(api, base, release) if release is not None else {}
        require(not set(assets) - set(manifest['files']), 'Unexpected existing GitHub release assets')
        for name, item in assets.items():
            require(item.get('digest') == 'sha256:' + manifest['files'][name],
                    'GitHub asset has different or unknown digest: ' + name)
        require(release is None or release.get('draft') or set(assets) == set(manifest['files']),
                'Existing public GitHub release is incomplete; review manually')
    else:
        links = release.get('assets', {}).get('links', []) if release is not None else []
        current = {}
        for link in links:
            require(link['name'] not in current and link['name'] in manifest['files'],
                    'Duplicate or unexpected GitLab release link')
            current[link['name']] = link['url']
        for name, sha in manifest['files'].items():
            package = package_path(base, manifest, name)
            require(name not in current or current[name] == 'https://' + api.host + package,
                    'GitLab release link destination differs: ' + name)
            existing = api.request('GET', package, optional=True, hash_body=True)
            require(existing is None or existing == sha, 'GitLab package bytes differ: ' + name)
            require(name not in current or existing == sha, 'GitLab release links to a missing package: ' + name)
    return release


def publish_repository(service, manifest, api, root=ROOT):
    base, path = release_paths(service, manifest)
    # Recheck conflicts after tag push, before the first upload.
    release = inspect_release(service, manifest, api)
    if service == 'github':
        if release is None:
            release = api.request('POST', base + '/releases', {
                'tag_name': manifest['tag'], 'target_commitish': manifest['commit'],
                'name': manifest['tag'], 'body': manifest['notes'], 'draft': True})
            require(release.get('draft') is True, 'GitHub did not create a draft release')
        assets = github_assets(api, base, release)
        upload = urlsplit(release['upload_url'].split('{')[0])
        require(upload.scheme == 'https' and upload.hostname == 'uploads.github.com'
                and not upload.username and not upload.password and not upload.port
                and not upload.query and not upload.fragment
                and upload.path == base + '/releases/' + str(release['id']) + '/assets',
                'Unexpected GitHub upload URL')
        for name, sha in manifest['files'].items():
            file = publication_folder(root) / name
            require(digest(file) == sha, 'Local file changed during publication: ' + name)
            if name not in assets:
                item = api.request('POST', upload.path + '?name=' + quote(name, safe=''), file=file, host=upload.hostname)
                require(item.get('digest') == 'sha256:' + sha, 'GitHub upload digest differs: ' + name)
        # Confirm the full remote inventory before transitioning the draft.
        actual = github_assets(api, base, release)
        require(set(actual) == set(manifest['files']) and all(
            actual[name].get('digest') == 'sha256:' + sha for name, sha in manifest['files'].items()),
            'GitHub final asset inventory or digests differ')
        if release.get('draft'):
            published = api.request('PATCH', base + '/releases/' + str(release['id']), {'draft': False})
            require(published.get('draft') is False, 'GitHub release is still a draft')
        return manifest['github'].rstrip('/') + '/releases/tag/' + manifest['tag']
    links = []
    for name, sha in manifest['files'].items():
        file = publication_folder(root) / name
        require(digest(file) == sha, 'Local file changed during publication: ' + name)
        package = package_path(base, manifest, name)
        existing = api.request('GET', package, optional=True, hash_body=True)
        require(existing is None or existing == sha, 'GitLab package bytes differ: ' + name)
        if existing is None:
            api.request('PUT', package, file=file)
        require(api.request('GET', package, hash_body=True) == sha, 'GitLab download verification failed: ' + name)
        links.append({'name': name, 'url': 'https://' + api.host + package, 'link_type': 'package'})
    if release is None:
        api.request('POST', base + '/releases', {'tag_name': manifest['tag'],
                    'name': manifest['tag'], 'description': manifest['notes'], 'assets': {'links': links}})
    else:
        current = {item['name']: item for item in release.get('assets', {}).get('links', [])}
        for link in links:
            if link['name'] not in current:
                api.request('POST', path + '/assets/links', link)
    final = inspect_release(service, manifest, api)
    require(final is not None and {x['name'] for x in final.get('assets', {}).get('links', [])} == set(manifest['files']),
            'GitLab did not confirm the complete release link inventory')
    return manifest['gitlab'].rstrip('/') + '/-/releases/' + manifest['tag']


def load_receipt(path, manifest_path):
    receipt = read_json(path) if path.exists() else {'manifest_sha256': digest(manifest_path), 'completed': {}}
    require(receipt.get('manifest_sha256') == digest(manifest_path), 'Receipt belongs to a different manifest')
    require(isinstance(receipt.get('completed'), dict) and not set(receipt['completed']) - set(SERVICES),
            'Invalid completed destination receipt')
    return receipt


def check_github_access(repo, project, api, token):
    require(repo.get('full_name', '').lower() == project.lower(),
            'GitHub repository identity differs from the configured destination')
    if not token.startswith('ghs_'):
        require(repo.get('permissions', {}).get('push') is True,
                'GitHub user credential does not report repository write access')
        return
    # Installation tokens have no user role. User-style push/pull flags are
    # not their Contents permission. Check installation access without writes;
    # GitHub remains authoritative for write authorization on each operation.
    page = 1
    while True:
        listing = api.request('GET', '/installation/repositories?per_page=100&page=' + str(page))
        entries = listing.get('repositories')
        require(isinstance(entries, list), 'Invalid GitHub installation repository response')
        for entry in entries:
            if entry.get('full_name', '').lower() == project.lower():
                require(not repo.get('id') or entry.get('id') == repo['id'],
                        'GitHub installation repository ID differs')
                return
        if len(entries) < 100:
            break
        page += 1
    raise ValueError('GitHub App installation does not include the configured repository')


def publish(services, confirmed=False, root=ROOT, config=None, tag=None, tag_message=None):
    require(confirmed, 'Review the manifest, then add --confirm-publish; no upload performed')
    path = root / 'outputs/release/manifest.json'
    manifest = read_json(path)
    require(manifest.get('version') == version(root) and release_tag(manifest.get('tag'), version(root)) == manifest.get('tag'), 'Version/tag mismatch')
    require(manifest.get('commit') == commit(root, clean=True) and manifest.get('source_sha256') == source_hash(root),
            'Source or commit changed; build and prepare again')
    require(tag is None or tag == manifest['tag'], 'Tag differs from prepared manifest')
    require(tag_message is None or tag_message == manifest.get('tag_message'), 'Tag annotation differs from prepared manifest')
    require(check(root) == manifest.get('inventory'), 'Prepared artifact inventory differs')
    verify_publication(manifest, root)
    receipt_path = path.parent / 'receipt.json'
    receipt = load_receipt(receipt_path, path)
    apis = {}
    # Read-only preflight for every selected destination before any tag mutation or upload.
    for service in services:
        host, project = repository(manifest[service], service == 'github')
        destination = git_destination(manifest, service)
        require('your-org' not in project and 'your-group' not in project and not host.endswith('.example.com'),
                'Replace example repository destinations before publication')
        if config is not None:
            require(manifest[service] == config[service] and destination == git_destination(config, service),
                    'Destination changed; prepare again')
        token = credential(service, config or {})
        _ACTIVE_SECRETS.add(token)
        secret_check(json.dumps(manifest))
        api = API('api.github.com' if service == 'github' else host, token, service == 'github')
        base, _ = release_paths(service, manifest)
        repo = api.request('GET', base)
        if service == 'github':
            check_github_access(repo, project, api, token)
        else:
            permissions = repo.get('permissions', {})
            level = max((permissions.get(k) or {}).get('access_level', 0)
                        for k in ('project_access', 'group_access'))
            require(repo.get('path_with_namespace') == project and level >= 30,
                    'GitLab repository identity or project access differs')
        current = remote_tag(destination, manifest['tag'], root)
        require(current is None or current == manifest['commit'], service + ': conflicting remote tag')
        state = inspect_release(service, manifest, api)
        if service in receipt['completed']:
            require(current == manifest['commit'] and state is not None, 'Completed destination is missing remotely')
            if service == 'github':
                require(state.get('draft') is False, 'Completed GitHub release is still a draft')
            else:
                require({x['name'] for x in state.get('assets', {}).get('links', [])} == set(manifest['files']),
                        'Completed GitLab release is incomplete')
        apis[service] = api
    tag = manifest['tag']
    local = run(['git', 'tag', '--list', tag], root, True)
    if local:
        require(run(['git', 'rev-parse', tag + '^{commit}'], root, True) == manifest['commit'], 'Conflicting local tag')
        require(run(['git', 'cat-file', '-t', 'refs/tags/' + tag], root, True) == 'tag', 'Local release tag must be annotated')
    write_json(receipt_path, receipt)
    if not local:
        run(['git', 'tag', '-a', tag, '-m', manifest.get('tag_message', 'LineClash ' + tag), manifest['commit']], root)
    for service in services:
        if service in receipt['completed']:
            print(service + ': already complete; verified remote state')
            continue
        receipt['in_progress'] = service
        write_json(receipt_path, receipt)
        # Repeat the local checks to catch source/artifact edits while networking.
        require(commit(root, clean=True) == manifest['commit'] and source_hash(root) == manifest['source_sha256']
                and inventory(root) == manifest['inventory'], 'Release inputs changed during publication')
        verify_publication(manifest, root)
        destination = git_destination(manifest, service)
        run(['git', 'push', '--no-follow-tags', destination, 'refs/tags/' + tag + ':refs/tags/' + tag], root)
        require(remote_tag(destination, tag, root) == manifest['commit'], 'Remote tag verification failed: ' + service)
        url = publish_repository(service, manifest, apis[service], root)
        receipt['completed'][service] = url
        receipt.pop('in_progress', None)
        write_json(receipt_path, receipt)
        print(service + ': ' + url)


def destinations(value):
    services = value.split(',')
    require(services and len(set(services)) == len(services) and not set(services) - set(SERVICES),
            'Choose distinct destinations from gitlab,github')
    return services


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command', choices=('source', 'plan', 'build', 'check', 'prepare', 'publish', 'release'))
    parser.add_argument('--config', type=Path, default=default_config_path())
    parser.add_argument('--notes', type=Path, default=ROOT / 'docs/RELEASE-NOTES.md')
    parser.add_argument('--to', default='gitlab,github')
    parser.add_argument('--confirm-publish', action='store_true')
    parser.add_argument('--confirm-push', action='store_true')
    parser.add_argument('--dry-run', action='store_true')
    parser.add_argument('--init', dest='initialize', action='store_true')
    parser.add_argument('--commit-message')
    parser.add_argument('--tag')
    parser.add_argument('--tag-message')
    parser.add_argument('--asset', type=Path, action='append', default=[], help='Additional prebuilt asset to include (repeatable, build only)')
    args = parser.parse_args(argv)
    if args.commit_message is not None and not (args.command == 'source' and args.initialize):
        parser.error('--commit-message requires source --init')
    if (args.dry_run or args.initialize or args.confirm_push) and args.command != 'source':
        parser.error('--dry-run, --init and --confirm-push are for source only')
    if args.confirm_publish and args.command not in ('publish', 'release', 'add-dmg'):
        parser.error('--confirm-publish is for publish/release only')
    if (args.tag is not None or args.tag_message is not None) and args.command not in ('prepare', 'publish', 'release', 'add-dmg'):
        parser.error('--tag and --tag-message are for prepare/publish/release')
    if args.asset and args.command != 'build':
        parser.error('--asset is for build only')
    config = configuration(args.config)
    if args.command not in ('plan', 'check'):
        require(args.config.expanduser().is_file(), 'Create external release settings first; see docs/RELEASING.md')
    if args.command == 'plan':
        print('Version: ' + version() + '; default annotated tag: ' + release_tag(version=version()))
        for service in SERVICES:
            print(service + ': ' + (config[service] or '(not configured)'))
        for command in build_commands(config):
            print(' '.join(command))
        print('Run native builds/runtime checks; package native binaries, source and website; record provenance and SHA256SUMS. No upload.')
    elif args.command == 'source':
        push_source(config, destinations(args.to), git_destination, args.confirm_push, args.dry_run,
                    initialize=args.initialize, commit_message=args.commit_message)
    elif args.command in ('publish', 'release'):
        selected = destinations(args.to)
        require(args.confirm_publish, 'Review release settings, then add --confirm-publish; nothing uploaded')
        if args.command == 'release':
            prepare(config, args.notes, tag=args.tag, tag_message=args.tag_message)
        publish(selected, args.confirm_publish, config=config, tag=args.tag, tag_message=args.tag_message)
    elif args.command == 'check':
        check()
        print('Source, provenance, native binaries, archives and exact checksum inventory passed.')
    elif args.command == 'build':
        build(config, assets=args.asset)
    else:
        prepare(config, args.notes, tag=args.tag, tag_message=args.tag_message)


if __name__ == '__main__':
    try:
        main()
    except (ValueError, OSError, subprocess.CalledProcessError, http.client.HTTPException, KeyError, RuntimeError) as error:
        if isinstance(error, subprocess.CalledProcessError):
            message = 'A release command failed (exit ' + str(error.returncode) + '); no automatic retry.'
        elif isinstance(error, KeyError):
            message = 'A release record or service response is incomplete.'
        else:
            message = str(error)
        print('Release stopped: ' + message, file=sys.stderr)
        sys.exit(1)
