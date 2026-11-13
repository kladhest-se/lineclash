#!/usr/bin/env python3
"""Verify bundled audio, attribution metadata, decoder sources and catalog."""
import argparse
import hashlib
import json
from pathlib import Path
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--write-catalog',action='store_true')
a=p.parse_args()
tracks=json.loads((root/'music/tracks.json').read_text())['tracks']
assert len(tracks)==19 and sum(t['role']=='menu' for t in tracks)==1
assert sum(t['source_url'].endswith('/sky-portal') for t in tracks)==1
assert sum(t['source_url'].endswith('/t-t-free-cyberpunk-pack-2') for t in tracks)==10
assert len({t['file'] for t in tracks})==len(tracks)
credits=(root/'CREDITS.md').read_text()
expected={t['file'] for t in tracks}|{t['intro_file'] for t in tracks if t.get('intro_file')}
actual={p.name for p in (root/'music').iterdir() if p.suffix.lower() in {'.mp3','.wav','.ogg'}}
assert actual==expected,'Uncredited or missing audio files in music/'
for t in tracks:
    for key in ['title','artist','license','license_url','source_url','file','attribution']:
        assert t[key] and t[key] in credits, (t['title'],key)
    for prefix in ['', 'intro_'] if t.get('intro_file') else ['']:
        path=root/'music'/t[prefix+'file']
        assert path.is_file() and not path.is_symlink()
        assert path.stat().st_size==t[prefix+'bytes']
        with path.open('rb') as f:
            h=hashlib.file_digest(f,'sha256') if hasattr(hashlib,'file_digest') else hashlib.sha256(f.read())
        assert h.hexdigest()==t[prefix+'sha256'],path
for item in json.loads((root/'clients/gui/third_party/SOURCES.json').read_text()):
    path=root/'clients/gui/third_party'/item['file']
    assert hashlib.sha256(path.read_bytes()).hexdigest()==item['sha256']
header='/* Generated from music/tracks.json. Keep credits and source links together. */\nstatic const MusicTrack music_catalog[]={\n'
for t in tracks:
    header+='    {'+', '.join(json.dumps(t.get(k,''),ensure_ascii=True) for k in ['title','artist','license','license_url','source_url','file','attribution','intro_file'])+'},\n'
header+='};\n'
path=root/'clients/gui/src/music_catalog.h'
if a.write_catalog:path.write_text(header)
assert path.read_text()==header,'Run scripts/check_music.py --write-catalog after editing metadata'
web_credits=root/'public-web/inc/music-credits.json'
fields=['title','artist','license','license_url','source_url','role','attribution','modifications']
expected_credits=[{key:track.get(key,'') for key in fields} for track in tracks]
if a.write_catalog:web_credits.write_text(json.dumps(expected_credits,ensure_ascii=False,indent=2)+'\n')
assert json.loads(web_credits.read_text())==expected_credits,'Run scripts/check_music.py --write-catalog to update website credits'
print(f'Verified {len(tracks)} credited selections and all original audio/decoder checksums')
