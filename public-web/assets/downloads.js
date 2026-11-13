/* Only published stable GitHub releases provide public downloads. */
(function(root){
'use strict';
const repo='https://github.com/kladhest-se/lineclash';
function classify(name){
 const text=name.toLowerCase();
 const platform=/source/.test(text)?'Source':/macos|darwin|osx|\.(dmg|pkg)$/.test(text)?'macOS':/windows|win32|win64|\.(exe|msi)$/.test(text)?'Windows':/linux|ubuntu|opensuse|\.(deb|rpm|appimage)$/.test(text)?'Linux':'Other';
 const kind=platform==='Source'?'Source code':/lineclashd|dedicated|server/.test(text)?'Dedicated server':'Game';
 const architecture=/arm64|aarch64/.test(text)?'ARM64':/x86_64|amd64|x64/.test(text)?'x64':/universal/.test(text)?'Universal':'';
 return {platform,kind,architecture};
}
function normalize(data){
 if(!data||data.draft!==false||data.prerelease!==false||typeof data.tag_name!=='string'||!data.tag_name.trim()||typeof data.published_at!=='string'||!Number.isFinite(Date.parse(data.published_at))||!Array.isArray(data.assets))throw Error('Invalid release');
 const assets=[],seen=new Set();
 for(const a of data.assets.slice(0,1000)){
  if(!a||a.state!=='uploaded'||typeof a.name!=='string'||!a.name||/[\x00-\x1f\x7f/\\]/.test(a.name)||!Number.isSafeInteger(a.size)||a.size<=0)continue;
  if(!/^lineclash[-_.]/i.test(a.name)||/website|checksum|sha256|provenance|manifest/i.test(a.name)||! /\.(zip|tar\.gz|tar\.xz|tgz|dmg|pkg|exe|msi|deb|rpm|appimage)$/i.test(a.name))continue;
  try{
   const u=new URL(a.browser_download_url),prefix='/kladhest-se/lineclash/releases/download/';
   if(u.origin!=='https://github.com'||u.username||u.password||!u.pathname.startsWith(prefix)||u.search||u.hash)continue;
   const parts=u.pathname.slice(prefix.length).split('/');
   if(parts.length!==2||decodeURIComponent(parts[0])!==data.tag_name||decodeURIComponent(parts[1])!==a.name||seen.has(a.name))continue;
   seen.add(a.name);assets.push({name:a.name,url:u.href,size:a.size,...classify(a.name)});
  }catch(_){/* Malformed and foreign asset URLs never become download links. */}
 }
 return {tag:data.tag_name,assets};
}
async function lookup(fetcher){
 const controller=new AbortController(),timer=setTimeout(()=>controller.abort(),6000);
 try{const response=await fetcher('https://api.github.com/repos/kladhest-se/lineclash/releases/latest',{headers:{Accept:'application/vnd.github+json'},credentials:'omit',signal:controller.signal});if(response.status===404)return {state:'unreleased'};if(!response.ok)throw Error('Release lookup failed');return {state:'released',release:normalize(await response.json())};}finally{clearTimeout(timer);}
}
if(typeof module==='object'&&module.exports)module.exports={normalize,lookup,classify};
if(typeof document==='undefined')return;
const status=document.getElementById('release-status'),list=document.getElementById('release-assets'),retry=document.getElementById('release-retry');if(!status)return;
let busy=false;
async function render(){if(busy)return;busy=true;retry.hidden=true;list.replaceChildren();status.textContent='Checking the latest release…';
 try{const result=await lookup(root.fetch.bind(root));
  if(result.state==='unreleased'){status.textContent='Not released yet. Build from source to play the development version.';return;}
  const release=result.release;status.textContent='Latest release · '+release.tag;
  const notes=document.createElement('a');notes.href=repo+'/releases/tag/'+encodeURIComponent(release.tag);notes.textContent='Release notes ↗';notes.className='section-link';list.append(notes);
  const groups=new Map();
  for(const asset of release.assets){
   const key=asset.platform+(asset.kind==='Dedicated server'?' · Dedicated server':'');
   if(!groups.has(key))groups.set(key,[]);groups.get(key).push(asset);
  }
  for(const [name,assets] of groups){
   const section=document.createElement('section');section.className='release-group';
   const heading=document.createElement('h3');heading.textContent=name;section.append(heading);
   for(const asset of assets){const link=document.createElement('a');link.href=asset.url;link.className='btn btn-outline';link.textContent=(asset.architecture?asset.architecture+' · ':'')+asset.name+' · '+(asset.size/1048576).toFixed(1)+' MB';section.append(link);}
   list.append(section);
  }
  if(!release.assets.length){const p=document.createElement('p');p.textContent='No downloadable packages are attached to this release. See the release notes or build from source.';list.append(p);}
 }catch(_){status.textContent='Latest release information is temporarily unavailable. Check GitHub releases or build from source.';retry.hidden=false;}finally{busy=false;}}
retry.addEventListener('click',render);render();
})(typeof window!=='undefined'?window:globalThis);
