"use strict";
(async()=>{
 const $=id=>document.getElementById(id), key=new URLSearchParams(location.search);
 key.set('action','replay');
 try {
  const response=await fetch('../api/?'+key); if(!response.ok)throw Error('Match unavailable ('+response.status+')');
  const data=await response.json(), g=data.match, events=data.events||[];
  $('summary').textContent=(Number(g.ranked)?'Ranked':'Casual')+' · '+g.end_reason+' · '+(Number(g.ended_epoch)?new Date(Number(g.ended_epoch)*1000).toISOString():g.ended_at+' (legacy time)')+' · Duration '+(Number(g.started_epoch)&&Number(g.ended_epoch)?Math.max(0,Number(g.ended_epoch)-Number(g.started_epoch))+'s':'unknown');
  for(const p of data.players){ const tr=document.createElement('tr'), td=document.createElement('td'), a=document.createElement('a'); a.textContent=p.nick;a.href='player.php?'+new URLSearchParams({server:g.server_id,nick:p.nick});td.append(a);tr.append(td);for(const value of [p.placement||'—',p.lines_cleared,p.specials_sent,p.blocks_placed]){const cell=document.createElement('td');cell.textContent=value;tr.append(cell);} $('players').append(tr); }
  if(!events.length){$('status').textContent='This match has no replay (older matches were not recorded).';return;}
  if(!crypto.subtle)throw Error('Replay checksum verification requires HTTPS or localhost');
  const digest=Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256',new TextEncoder().encode(data.replay_source)))).map(n=>n.toString(16).padStart(2,'0')).join('');
  if(digest!==g.replay_sha256)throw Error('Replay checksum mismatch — playback refused');
  $('status').textContent='Checksum verified. '+(Number(g.replay_truncated)?'Partial trace: recording limit reached.':'Complete recorded trace.');
  key.set('download','1');$('download').href='../api/?'+key;
  const duration=events[events.length-1].elapsed_ms, seek=$('seek'), ctx=$('board').getContext('2d');seek.max=duration;$('play').disabled=false;
  let playing=false, position=0, previous=0;
  const colors=['#232634','#e78284','#a6d189','#8caaee','#e5c890','#ca9ee6','#81c8be','#ef9f76','#babbf1','#737994'];
  function render(){const boards={},tail=[];for(const e of events){if(e.elapsed_ms>position)break;const match=/^f ([1-8]) ([0-9a-z]{264})$/.exec(e.line);if(match)boards[match[1]]=match[2];else tail.push((e.elapsed_ms/1000).toFixed(1)+'s '+e.line);}ctx.fillStyle='#232634';ctx.fillRect(0,0,1008,440);for(let slot=1;slot<=8;slot++){const x=(slot-1)*126;ctx.fillStyle='#c6d0f5';ctx.font='12px sans-serif';ctx.fillText('Player '+slot,x+3,15);const field=boards[slot]||'0'.repeat(264);for(let i=0;i<264;i++){const n=parseInt(field[i],36)||0;ctx.fillStyle=colors[n%colors.length];ctx.fillRect(x+(i%12)*10,22+Math.floor(i/12)*18,9,17);}}$('events').textContent=tail.slice(-8).join('\n');$('time').textContent=(position/1000).toFixed(1)+' / '+(duration/1000).toFixed(1)+'s';seek.value=position;}
  $('play').onclick=()=>{if(position>=duration)position=0;playing=!playing;$('play').textContent=playing?'Pause':'Play';};seek.oninput=()=>{position=Number(seek.value);render();};
  function frame(now){if(playing){position=Math.min(duration,position+(previous?now-previous:0)*Number($('speed').value));if(position>=duration){playing=false;$('play').textContent='Play';}render();}previous=now;requestAnimationFrame(frame);}render();requestAnimationFrame(frame);
 }catch(e){$('status').textContent=e.message;}
})();
