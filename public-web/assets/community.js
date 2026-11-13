"use strict";
document.addEventListener('click',async event=>{const button=event.target.closest('[data-copy-address]');if(!button)return;const feedback=button.closest('.server-card').querySelector('.copy-feedback');try{await navigator.clipboard.writeText(button.dataset.copyAddress);feedback.textContent='Address copied';button.textContent='Copied';}catch{feedback.textContent='Select and copy the address above.';}});

// Keep the contents selection in sync with clicks, deep links and history.
(() => {
 const links=[...document.querySelectorAll('.documentation-toc a[href^="#"]')];
 if(!links.length)return;
 function selectChapter(){
  let target=null;
  try { target=document.getElementById(decodeURIComponent(location.hash.slice(1))); } catch (_) {}
  const chapter=target?.closest('.documentation-chapter');
  const selected=chapter?'#'+chapter.id:location.hash?'':links[0].hash;
  for(const link of links){
   if(link.hash===selected)link.setAttribute('aria-current','location');
   else link.removeAttribute('aria-current');
  }
 }
 addEventListener('hashchange',selectChapter);
 selectChapter();
})();
