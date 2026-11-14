/* SPDX-License-Identifier: MIT */
#ifndef LC_TVOS_FOCUS_H
#define LC_TVOS_FOCUS_H
typedef struct { int x,y,w,h; } LCFocusRect;
void lc_focus_begin(int context);
void lc_focus_add(int x,int y,int w,int h);
void lc_focus_finish(void);
void lc_focus_move(int dx,int dy);
int lc_focus_current(LCFocusRect *rect);
#endif
