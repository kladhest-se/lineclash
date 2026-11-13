"""Native installer production; signing and missing tools fail closed."""
import hashlib
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import tempfile
import uuid
import xml.etree.ElementTree as ET


def call(args, **kwargs):
    return subprocess.run([str(a) for a in args],check=True,**kwargs)


def preflight(env):
    system=platform.system()
    if system=='Windows':
        if not shutil.which('wix'):raise RuntimeError('WiX CLI is required for MSI output. Install the .NET SDK, then: dotnet tool install --global wix --version 4.0.6')
        return ''
    if system!='Darwin':return ''
    identity=env.get('SIGN_IDENTITY','').strip()
    if identity=='-':raise RuntimeError('Release builds require Developer ID signing, not ad-hoc signing')
    text=call(['security','find-identity','-v','-p','codesigning'],capture_output=True,text=True).stdout
    found=re.findall(r'\b([0-9A-F]{40}) "(Developer ID Application:[^"]+)"',text)
    if identity:
        matches=[fingerprint for fingerprint,name in found if identity in (fingerprint,name)]
    else:matches=[fingerprint for fingerprint,name in found]
    if len(matches)!=1:raise RuntimeError('Set SIGN_IDENTITY to one available Developer ID Application identity; use security find-identity -v -p codesigning')
    return matches[0]


def sign_binaries(root,identity):
    for rel in ['clients/gui/lineclash','server/lineclashd','server/lineclashd-tool']:
        binary=root/rel
        call(['codesign','--force','--timestamp','--options','runtime','--sign',identity,binary])
        call(['codesign','--verify','--strict',binary])


def wix_document(entries,version):
    """One stable component per file, embedded CAB, normal uninstall/upgrade."""
    if not re.fullmatch(r'\d+\.\d+\.\d+',version):raise ValueError('MSI requires a numeric major.minor.patch release version')
    major,minor,patch=map(int,version.split('.'))
    if major>255 or minor>255 or patch>65535:raise ValueError('Version exceeds Windows Installer limits')
    ns='http://wixtoolset.org/schemas/v4/wxs';ET.register_namespace('',ns)
    def node(parent,tag,**attrs):return ET.SubElement(parent,'{'+ns+'}'+tag,attrs)
    wix=ET.Element('{'+ns+'}Wix')
    package=node(wix,'Package',Name='LineClash',Manufacturer='LineClash',Version=version,UpgradeCode='CC02F204-7C75-4D84-B31C-9C027722B989',Scope='perMachine',InstallerVersion='500')
    node(package,'MajorUpgrade',DowngradeErrorMessage='A newer version of LineClash is already installed.')
    node(package,'MediaTemplate',EmbedCab='yes')
    base=node(package,'StandardDirectory',Id='ProgramFiles64Folder')
    directories={'':node(base,'Directory',Id='INSTALLFOLDER',Name='LineClash')}
    node(package,'StandardDirectory',Id='ProgramMenuFolder')
    feature=node(package,'Feature',Id='Game',Title='LineClash',Level='1')
    seen=set()
    for source,name in entries:
        path=Path(name)
        if path.is_absolute() or '..' in path.parts or name.casefold() in seen:raise ValueError('Invalid or duplicate installer path')
        seen.add(name.casefold());parent=''
        for part in path.parts[:-1]:
            key=parent+'/'+part if parent else part
            if key not in directories:
                directories[key]=node(directories[parent],'Directory',Id='D'+hashlib.sha256(key.encode()).hexdigest()[:30],Name=part)
            parent=key
        ident=hashlib.sha256(name.encode()).hexdigest()[:30]
        component=node(directories[parent],'Component',Id='C'+ident,Guid=str(uuid.uuid5(uuid.NAMESPACE_URL,'https://lineclash.kladhest.se/install/'+name)))
        file=node(component,'File',Id='F'+ident,Source=str(Path(source).resolve()),Name=path.name,KeyPath='yes')
        if name=='bin/lineclash.exe':node(file,'Shortcut',Id='StartGame',Directory='ProgramMenuFolder',Name='LineClash',Advertise='yes',WorkingDirectory=directories[parent].get('Id'))
        node(feature,'ComponentRef',Id='C'+ident)
    return ET.tostring(wix,encoding='utf-8',xml_declaration=True)


def installers(root,folder,version,identity,env):
    system=platform.system()
    if system=='Darwin':
        if not identity:raise RuntimeError('Developer ID signing is required for macOS releases')
        target=root/'dist'/f'LineClash-{version}-macos-{platform.machine()}.dmg'
        if target.exists():raise RuntimeError('Preserve existing DMG before rebuilding: '+str(target))
        call(['bash','scripts/package-macos.sh','--sign',identity],cwd=root,env=env)
        dest=folder/target.name;shutil.copyfile(target,dest)
        call(['codesign','--verify','--strict',dest])
        zip_source=target.with_suffix('.zip');zip_dest=folder/zip_source.name
        shutil.copyfile(zip_source,zip_dest)
        return [dest]
    if system=='Windows':
        from release import native_files
        with tempfile.TemporaryDirectory(prefix='lineclash-msi-') as temp:
            temp=Path(temp);wxs=temp/'LineClash.wxs'
            wxs.write_bytes(wix_document([(root/path,name) for path,name in native_files(root)],version))
            target=temp/f'LineClash-{version}-windows-{platform.machine()}.msi'
            call(['wix','build','-arch','x64','-o',target,wxs],cwd=root,env=env)
            dest=folder/target.name;shutil.copyfile(target,dest)
        return [dest]
    return []


def verify_installers(record,folder,root):
    # Legacy build records remain readable; new builds always record this field.
    if 'installer_assets' not in record:return
    files=record['installer_assets'];system=record['platform']
    extension='.dmg' if system=='Darwin' else '.msi' if system=='Windows' else None
    if len(files)!=(1 if extension else 0) or any(not name.endswith(extension) for name in files):raise RuntimeError('Missing required native installer')
    for name in files:
        if name not in record['artifacts']:raise RuntimeError('Installer absent from provenance')
        if system=='Windows':
            with (folder/name).open('rb') as stream:
                if stream.read(8)!=bytes.fromhex('d0cf11e0a1b11ae1'):raise RuntimeError('Invalid MSI compound file')
        else:
            if not record.get('signing_identity'):raise RuntimeError('Missing macOS signing identity')
            call(['codesign','--verify','--strict',folder/name])
            for rel in ['clients/gui/lineclash','server/lineclashd','server/lineclashd-tool']:
                call(['codesign','--verify','--strict',root/rel])
