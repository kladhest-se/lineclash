/* SPDX-License-Identifier: MIT
 * Spatial focus for the immediate-mode SDL interface. Store rectangles, not
 * pointers to ephemeral widgets. Rebuild the registry each rendered frame. */
#include "focus.h"
#include <limits.h>
static LCFocusRect items[256],selected;
static int count,valid,context=-1;
static int equal(LCFocusRect a,LCFocusRect b){return a.x==b.x && a.y==b.y && a.w==b.w && a.h==b.h;}
void lc_focus_begin(int next){count=0;if(next!=context){valid=0;context=next;}}
void lc_focus_add(int x,int y,int w,int h){if(count<256 && w>0 && h>0)items[count++]=(LCFocusRect){x,y,w,h};}
void lc_focus_finish(void){
    for(int i=0;i<count;i++)if(valid && equal(selected,items[i]))return;
    valid=count>0;if(valid)selected=items[0];
}
int lc_focus_current(LCFocusRect *rect){if(valid)*rect=selected;return valid;}
void lc_focus_move(int dx,int dy){
    if(!valid)return;
    int best=-1;long long score=LLONG_MAX;
    for(int i=0;i<count;i++){
        int x=2*(items[i].x-selected.x)+items[i].w-selected.w;
        int y=2*(items[i].y-selected.y)+items[i].h-selected.h;
        int forward=x*dx+y*dy;
        int low=dy?selected.x:selected.y,high=low+(dy?selected.w:selected.h);
        int next_low=dy?items[i].x:items[i].y,next_high=next_low+(dy?items[i].w:items[i].h);
        int side=next_low>high?2*(next_low-high):low>next_high?2*(low-next_high):0;
        if(forward<=0)continue;
        long long s=(long long)forward*forward+4LL*side*side;
        if(s<score){score=s;best=i;}
    }
    if(best>=0)selected=items[best];
}
