'use strict';
const assert=require('node:assert/strict');
const {normalize,lookup}=require('../public-web/assets/downloads.js');
const asset=(name,url='https://github.com/kladhest-se/lineclash/releases/download/v1/'+name)=>({name,browser_download_url:url,state:'uploaded',size:123});
const release={draft:false,prerelease:false,tag_name:'v1',published_at:'2026-11-13T12:00:00Z',assets:[asset('lineclash-macos.zip'),asset('lineclash-website.tar.gz'),asset('bad.zip','https://evil.example/bad.zip'),asset('empty.zip')]};release.assets[3].size=0;
assert.deepEqual(normalize(release).assets.map(a=>a.name),['lineclash-macos.zip']);
assert.throws(()=>normalize({...release,draft:true}));assert.throws(()=>normalize({...release,prerelease:true}));assert.throws(()=>normalize({...release,published_at:'bad'}));
assert.equal(normalize({...release,assets:[]}).assets.length,0);
(async()=>{assert.deepEqual(await lookup(async()=>({status:404})),{state:'unreleased'});assert.equal((await lookup(async()=>({ok:true,json:async()=>release}))).release.tag,'v1');for(const status of [403,429,500])await assert.rejects(()=>lookup(async()=>({ok:false,status})));await assert.rejects(()=>lookup(async()=>{throw Error('offline')}));await assert.rejects(()=>lookup(async()=>({ok:true,json:async()=>({})})));console.log('PASS: published release downloads, absent releases, unsafe assets and lookup failures');})().catch(e=>{console.error(e);process.exitCode=1;});
