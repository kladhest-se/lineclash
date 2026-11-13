#!/usr/bin/env python3
"""Generate the self-contained website manual from canonical project guides."""
import argparse
import html
from html.parser import HTMLParser
from pathlib import Path
import posixpath
import re
from urllib.parse import urlsplit,unquote
try:
 import markdown
 from markdown.extensions.toc import slugify
except ImportError:
 raise SystemExit('Install documentation build dependencies: python3 -m pip install -r scripts/requirements-docs.txt')
ROOT=Path(__file__).resolve().parents[1]
PROFILES={
 'basics':[('playing','Game basics','docs/GAME-BASICS.md'),('modes','Game modes','docs/GAME-MODES.md'),('specials','Specials','docs/SPECIALS.md'),('features','Servers, history & replays','docs/GAMEPLAY-FEATURES.md')],
 'controls':[('clients','Run & configure clients','docs/CLIENTS.md'),('controls','Controls & accessibility','docs/CONTROLS.md')],
 'server':[('server-start','Start a server','docs/SERVER-START.md'),('configuration','Configure a server','docs/CONFIGURATION.md'),('management','Manage a server','docs/SERVER-MANAGEMENT.md')],
 'source':[('development','Run from source','docs/DEVELOPMENT.md'),('windows','Build & run on Windows','docs/WINDOWS.md')],
 'api':[('community-api','Community API','docs/COMMUNITY-API.md'),('status-api','Dedicated server API','docs/API.md')],
}
CHAPTERS={path:(key,profile) for profile,guides in PROFILES.items() for key,title,path in guides}
DOC_PAGES={'basics':'Gameplay','controls':'Clients & controls','server':'Server setup'}
BASES={profile:('../../' if profile in DOC_PAGES or profile=='api' else '../') for profile in PROFILES}
ROUTES={profile:'documentation/'+profile+'/' for profile in DOC_PAGES}
ROUTES.update(source='source/',api='api/docs/')
TARGETS={profile:profile+'-documentation.html' for profile in PROFILES}
ASSETS={}

def link(url,source,profile):
 parsed=urlsplit(html.unescape(url))
 if parsed.scheme and parsed.scheme not in ('http','https','mailto'):return '#'
 path=unquote(parsed.path)
 if parsed.netloc=='github.com' and path.startswith('/kladhest-se/lineclash/blob/main/'):
  path=path.removeprefix('/kladhest-se/lineclash/blob/main/');parsed=parsed._replace(scheme='',netloc='')
 elif parsed.scheme or parsed.netloc:return url
 elif not path:return '#'+CHAPTERS[source][0]+'-'+parsed.fragment if parsed.fragment else '#'+CHAPTERS[source][0]
 else:path=posixpath.normpath(posixpath.join(posixpath.dirname(source),path))
 if path in CHAPTERS:
  chapter,destination=CHAPTERS[path]
  prefix='' if profile==destination else BASES[profile]+ROUTES[destination]
  return prefix+'#'+chapter+('-'+parsed.fragment if parsed.fragment else '')
 if path in ('CREDITS.md','music/README.md'):return BASES[profile]+'credits/'
 if path=='LICENSE.md':return BASES[profile]+'source/'
 if path.startswith('docs/') and path.endswith('.md'):return BASES[profile]+'documentation/'
 candidate=ROOT/path
 if candidate.is_file() and candidate.suffix.lower() in ('.png','.jpg','.jpeg','.svg'):
  ASSETS[candidate.name]=candidate
  return BASES[profile]+'assets/documentation/'+candidate.name
 # Repository source links remain available; guide links stay within this manual.
 return 'https://github.com/kladhest-se/lineclash/blob/main/'+path

class SafeHTML(HTMLParser):
 allowed=set('p h2 h3 h4 h5 h6 ul ol li strong em code pre blockquote table thead tbody tr th td hr br a img del'.split())
 def __init__(self,source,profile):super().__init__(convert_charrefs=True);self.source=source;self.profile=profile;self.parts=[]
 def handle_starttag(self,tag,attrs):
  if tag not in self.allowed:return
  kept=[]
  for key,value in attrs:
   if key not in ('id','class','href','src','alt','title'):continue
   if key in ('href','src'):value=link(value or '',self.source,self.profile)
   kept.append(' '+key+'="'+html.escape(value or '',quote=True)+'"')
  if tag=='img':
   kept.append(' loading="lazy"')
   # Reserve image space so lazy loading cannot shift a selected chapter.
   source=ASSETS.get(Path(urlsplit(dict(attrs).get('src','')).path).name)
   if source:
    data=source.read_bytes()[:24]
    if data[:8]==b'\x89PNG\r\n\x1a\n':
     width=int.from_bytes(data[16:20],'big');height=int.from_bytes(data[20:24],'big')
     kept.append(' width="'+str(width)+'" height="'+str(height)+'"')
  self.parts.append('<'+tag+''.join(kept)+'>')
 def handle_endtag(self,tag):
  if tag in self.allowed and tag not in ('hr','br','img'):self.parts.append('</'+tag+'>')
 def handle_data(self,data):self.parts.append(html.escape(data,quote=False))

def public_text(text):
 replacements={'lineclash.kladhest.se':'lineclash.example.org','sth-ts-web002.kladhest.se':'web.example.org','sth-ts-dbs001.kladhest.se':'db.example.org','sth-ts-mgt001.kladhest.se':'game.example.org','172.16.1.132':'192.0.2.20','172.16.1.121':'192.0.2.30','172.16.1.100':'192.0.2.40','sth-ts-web002':'web-host','sth-ts-dbs001':'db-host','sth-ts-mgt001':'game-host','/Users/frossmant/Development/tools/git-release/projects/lineclash.json':'/path/to/git-release/projects/lineclash.json'}
 for before,after in replacements.items():text=text.replace(before,after)
 return text.replace('This guide is tailored to your topology.','This guide uses an example topology; adapt the hosts and addresses to your infrastructure.')

def public_guide(path):
 text=(ROOT/path).read_text()
 if path=='docs/PLAYING.md':
  text='\n\n'.join(p for p in text.split('\n\n') if not p.startswith(('The GUI music player','See [discovery')))
  text=text.replace(' See [trust review](TRUST-REVIEW.md) and [platform verification](PLATFORM-VERIFICATION.md).','')
 elif path=='docs/CONFIGURATION.md':
  text='# Configure a game server\n\nSettings are read at startup. Restart after editing the configuration. Room settings saved in SQLite can override initial defaults; use room controls to update an existing room.\n\n## File format and options'+text.split('## File format and options',1)[1].split('## Accounts and administration',1)[0]
 elif path=='docs/DEVELOPMENT.md':
  text="""# Run from source

After building with the platform commands above, run the server and client in separate terminals from the project root.

## Start a local server — macOS and Linux

Create a local configuration with writable database and log paths:

```sh
cp server/lineclashd.cfg.example ./lineclashd.cfg
./server/lineclashd --config ./lineclashd.cfg
```

Leave this terminal open. Press Ctrl+C to stop the server. To restart it, run the same command again with the same configuration.

## Start the desktop client — macOS and Linux

In another terminal, from the project root:

```sh
./clients/gui/lineclash
```

Add `localhost` with game port `31457`, enter a nickname and password, connect and join a room. A new nickname registers when server registration is enabled. Use Quit or close the window to stop the client.

## Administration tool

```sh
./server/lineclashd-tool --help
```

On Windows, use `./server/lineclashd-tool.exe --help`. See the [server guide](SERVER-MANAGEMENT.md) for account privileges and administration. See [clients and controls](CLIENTS.md) for preferences and connection troubleshooting.

## Rebuild after changes

Run `make -j4` again from the project root. Stop running programs before replacing their binaries. For a clean rebuild, run `make clean` followed by `make -j4`. Keep your configuration and database when rebuilding.
"""
 elif path=='docs/WINDOWS.md':
  text=text.split('For verification and a distributable ZIP:')[0]
  text=text.replace('# Windows build and packaging','# Build and run on Windows')
  text+='\n## Stop and restart\n\nStop the foreground server with Ctrl+C in its terminal. Close the game window to exit the client. Start either executable again with the same command. Connect the GUI to `localhost:31457` when using the local server.\n'
 elif path=='docs/API.md':text=text.split('## Public directory API')[0]
 elif path=='docs/COMMUNITY-API.md':
  text=text.split('The directory supervisor handles')[0]+'Keep tokens private and use HTTPS.\n'
 return public_text(text)

def generate(profile):
 guides=PROFILES[profile]
 nav=''.join('<li><a href="#'+key+'">'+html.escape(title)+'</a></li>' for key,title,path in guides)
 parts=['<!-- Generated by scripts/build_web_docs.py. Edit the Markdown guides, then rebuild. -->','<div class="documentation-layout"><aside class="documentation-toc"><details open><summary>On this page</summary><nav aria-label="Documentation contents"><ul>'+nav+'</ul></nav></details></aside><div class="documentation-content">']
 if profile in DOC_PAGES:
  parts[1]='<div class="documentation-layout documentation-without-sidebar"><div class="documentation-content">'
 for key,title,path in guides:
  rendered=markdown.markdown(public_guide(path),extensions=['fenced_code','tables','toc','sane_lists'],extension_configs={'toc':{'baselevel':2,'slugify':lambda value,sep,prefix=key:prefix+'-'+slugify(value,sep)}})
  parser=SafeHTML(path,profile);parser.feed(rendered);parser.close()
  parts.append('<section class="documentation-chapter" id="'+key+'">'+''.join(parser.parts)+'<p class="back-to-contents"><a href="#main">Back to contents ↑</a></p></section>')
 parts.append('</div></div>')
 return '\n'.join(parts)+'\n'

def main():
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--check',action='store_true');args=parser.parse_args()
 for profile,filename in TARGETS.items():
  output=generate(profile);target=ROOT/'public-web/inc'/filename
  if args.check:
   if not target.is_file() or target.read_text()!=output:raise SystemExit('Website documentation is stale; run scripts/build_web_docs.py')
  else:target.write_text(output)
 for name,source in ASSETS.items():
  destination=ROOT/'public-web/assets/documentation'/name
  if args.check:
   if not destination.is_file() or destination.read_bytes()!=source.read_bytes():raise SystemExit('Stale documentation image: '+name)
  else:destination.parent.mkdir(parents=True,exist_ok=True);destination.write_bytes(source.read_bytes())
 print(('Checked' if args.check else 'Generated')+' '+str(sum(map(len,PROFILES.values())))+' documentation chapters across '+str(len(PROFILES))+' pages and '+str(len(ASSETS))+' images')
if __name__=='__main__':main()
