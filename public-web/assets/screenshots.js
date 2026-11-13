(() => {
 'use strict';
 const dialog=document.querySelector('.screenshot-viewer');
 if(!dialog || typeof dialog.showModal!=='function')return;
 const image=dialog.querySelector('img'),caption=dialog.querySelector('#screenshot-caption'),original=dialog.querySelector('[data-viewer-original]'),zoom=dialog.querySelector('[data-viewer-zoom]'),stage=dialog.querySelector('.viewer-stage');
 let opener=null;
 document.addEventListener('click',event=>{
  const link=event.target.closest('a[data-screenshot]');
  if(!link || event.button!==0 || event.metaKey || event.ctrlKey || event.shiftKey || event.altKey)return;
  event.preventDefault();opener=link;image.src=link.href;image.alt=link.dataset.caption;caption.textContent=link.dataset.caption;original.href=link.href;
  stage.classList.remove('actual-size');zoom.setAttribute('aria-pressed','false');zoom.textContent='Actual size';dialog.showModal();document.documentElement.classList.add('screenshot-open');stage.scrollTop=stage.scrollLeft=0;
 });
 dialog.querySelector('[data-viewer-close]').addEventListener('click',()=>dialog.close());
 dialog.addEventListener('click',event=>{if(event.target!==dialog)return;const r=dialog.getBoundingClientRect();if(event.clientX<r.left || event.clientX>r.right || event.clientY<r.top || event.clientY>r.bottom)dialog.close();});
 zoom.addEventListener('click',()=>{const actual=stage.classList.toggle('actual-size');zoom.setAttribute('aria-pressed',String(actual));zoom.textContent=actual?'Fit to screen':'Actual size';});
 dialog.addEventListener('close',()=>{document.documentElement.classList.remove('screenshot-open');image.removeAttribute('src');if(opener)opener.focus({preventScroll:true});});
})();
