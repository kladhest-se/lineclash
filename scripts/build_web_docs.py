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
 'manual':[('playing','Playing','docs/PLAYING.md'),('features','Servers, history & replays','docs/GAMEPLAY-FEATURES.md'),('configuration','Server configuration','docs/CONFIGURATION.md')],
 'source':[('development','Build & development','docs/DEVELOPMENT.md'),('windows','Windows builds','docs/WINDOWS.md')],
 'api':[('community-api','Community API','docs/COMMUNITY-API.md'),('status-api','Dedicated server API','docs/API.md')],
}
CHAPTERS={path:(key,profile) for profile,guides in PROFILES.items() for key,title,path in guides}
BASES={'manual':'../','source':'../','api':'../../'}
ROUTES={'manual':'documentation/','source':'source/','api':'api/docs/'}
TARGETS={'manual':'documentation.html','source':'source-documentation.html','api':'api-documentation.html'}
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
  text=text.split('## Public website registration')[0]
 elif path=='docs/DEVELOPMENT.md':
  packages=text.split('## Packages',1)[1].split('## Public web page',1)[0]
  text='# Build and development\n\nBuild the native game and dedicated server with a C compiler, make, SQLite and SDL2. macOS and Linux also use libcurl; Windows uses WinHTTP. Platform-specific commands are above.\n\n## Project layout\n\nThe GUI is in `clients/gui/`, the dedicated server in `server/`, and shared gameplay/network code in `shared/`. Run `make -j4` from the project root to build. The default Windows target excludes the POSIX terminal client.\n\n## Packages'+packages
  text+='\n## Interface development\n\nThe antialiased GUI font is compiled from `clients/gui/src/ui_font.h`. Regenerate it with Pillow and `python3 scripts/build_ui_font.py`. Roboto is licensed under SIL OFL 1.1; preserve its license when packaging.\n'
 elif path=='docs/WINDOWS.md':
  text=text.split('Verification evidence:')[0]
  text=text.replace('For verification and a distributable ZIP:','Create a distributable ZIP:').replace('make verify\n','')
 elif path=='docs/API.md':text=text.split('## Public directory API')[0]
 elif path=='docs/COMMUNITY-API.md':
  text=text.split('The directory supervisor handles')[0]+'Keep tokens private and use HTTPS.\n'
 return public_text(text)

def generate(profile):
 guides=PROFILES[profile]
 nav=''.join('<li><a href="#'+key+'">'+html.escape(title)+'</a></li>' for key,title,path in guides)
 parts=['<!-- Generated by scripts/build_web_docs.py. Edit the Markdown guides, then rebuild. -->','<div class="documentation-layout"><aside class="documentation-toc"><details open><summary>On this page</summary><nav aria-label="Documentation contents"><ul>'+nav+'</ul></nav></details></aside><div class="documentation-content">']
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
 print(('Checked' if args.check else 'Generated')+' '+str(sum(map(len,PROFILES.values())))+' documentation chapters across 3 pages and '+str(len(ASSETS))+' images')
if __name__=='__main__':main()
