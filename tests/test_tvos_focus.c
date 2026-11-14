/* SPDX-License-Identifier: MIT */
#include "../clients/tvos/focus.h"
#include <assert.h>
int main(void){
    LCFocusRect r;
    lc_focus_begin(1);lc_focus_add(0,0,20,20);lc_focus_add(30,0,20,20);lc_focus_add(0,40,100,20);lc_focus_finish();
    assert(lc_focus_current(&r) && r.x==0 && r.y==0);
    lc_focus_move(1,0);assert(lc_focus_current(&r) && r.x==30);
    lc_focus_move(0,1);assert(lc_focus_current(&r) && r.y==40);
    lc_focus_move(0,1);assert(lc_focus_current(&r) && r.y==40);
    lc_focus_begin(1);lc_focus_add(0,40,100,20);lc_focus_finish();assert(lc_focus_current(&r) && r.y==40);
    lc_focus_begin(2);lc_focus_add(10,10,20,20);lc_focus_finish();assert(lc_focus_current(&r) && r.x==10);
    lc_focus_begin(2);lc_focus_finish();assert(!lc_focus_current(&r));
    lc_focus_begin(3);lc_focus_add(900,22,124,32);lc_focus_add(320,320,420,38);lc_focus_add(535,626,205,32);lc_focus_finish();
    lc_focus_move(0,1);assert(lc_focus_current(&r) && r.y==320);
    return 0;
}
