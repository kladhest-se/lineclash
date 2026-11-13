(() => {
  'use strict';
  const canvas=document.getElementById('block-background'); if(!canvas)return;
  const ctx=canvas.getContext('2d'); if(!ctx)return;
  const motion=matchMedia('(prefers-reduced-motion: reduce)');
  const colors=['#99d1db','#e5c890','#ca9ee6','#a6d189','#e78284','#8caaee','#ef9f76'];
  let pieces=[],width=0,height=0,frame=0,last=0;
  function piece(randomY=false){return {x:Math.random()*width,y:randomY?Math.random()*height:-100,w:28*(1+Math.floor(Math.random()*4))-2,h:28*(1+Math.floor(Math.random()*3))-2,s:15+Math.random()*27,c:colors[Math.floor(Math.random()*colors.length)]};}
  function resize(){width=innerWidth;height=innerHeight;const dpr=Math.min(devicePixelRatio||1,2);canvas.width=width*dpr;canvas.height=height*dpr;ctx.setTransform(dpr,0,0,dpr,0,0);pieces=Array.from({length:Math.ceil(width/48)},()=>piece(true));restart();}
  function draw(time){frame=0;const dt=last?Math.min((time-last)/1000,.05):0;last=time;ctx.clearRect(0,0,width,height);pieces.forEach((p,i)=>{if(!motion.matches)p.y+=p.s*dt;ctx.fillStyle=p.c;ctx.fillRect(p.x,p.y,p.w,p.h);if(p.y>height)pieces[i]=piece();});if(!document.hidden&&!motion.matches)frame=requestAnimationFrame(draw);}
  function restart(){cancelAnimationFrame(frame);last=0;if(!document.hidden)draw(performance.now());}
  addEventListener('resize',resize);document.addEventListener('visibilitychange',restart);motion.addEventListener('change',restart);resize();
})();
