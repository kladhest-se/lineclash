#include "client.h"
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#if defined(PLATFORM_WINDOWS)
#  include <shellapi.h>  /* ShellExecuteA */
#endif

static SDL_GameController *g_controller=NULL;
static Uint32 g_pad_buttons=0,g_pad_repeat[3]={0};
static int g_pad_context=-1;
static int g_patterns=0,g_high_contrast=0,g_ui_scale=100;
static SDL_Window   *g_win  = NULL;
static SDL_Renderer *g_rend = NULL;
static SDL_Texture *g_ui_font = NULL;
#include "ui_font.h"
static int g_popup_blocked = 0; /* when 1: dbtn suppresses hover+click */
static MouseState    g_mouse = {0,0,0,0};
static uint32_t      g_rtick = 0; /* increments every render frame */

/* ============================== FONT ============================== */
#define FW 7
#define FH 13
#define NCHARS 256
static const unsigned char FONT[NCHARS][FH]={
[0x20]={0,0,0,0,0,0,0,0,0,0,0,0,0},
[0x21]={0,8,8,8,8,8,8,0,8,0,0,0,0},[0x22]={0,20,20,20,0,0,0,0,0,0,0,0,0},
[0x23]={0,20,20,62,20,62,20,20,0,0,0,0,0},[0x24]={0,8,30,40,28,10,60,8,0,0,0,0,0},
[0x25]={0,33,34,4,8,16,34,33,0,0,0,0,0},[0x26]={0,16,40,40,16,41,34,29,0,0,0,0,0},
[0x27]={0,8,8,8,0,0,0,0,0,0,0,0,0},[0x28]={0,4,8,16,16,16,8,4,0,0,0,0,0},
[0x29]={0,16,8,4,4,4,8,16,0,0,0,0,0},[0x2a]={0,0,8,42,28,42,8,0,0,0,0,0,0},
[0x2b]={0,0,8,8,62,8,8,0,0,0,0,0,0},[0x2c]={0,0,0,0,0,0,12,8,16,0,0,0,0},
[0x2d]={0,0,0,0,62,0,0,0,0,0,0,0,0},[0x2e]={0,0,0,0,0,0,12,12,0,0,0,0,0},
[0x2f]={0,1,2,4,8,16,32,64,0,0,0,0,0},
[0x30]={0,28,34,35,37,41,49,28,0,0,0,0,0},[0x31]={0,8,24,8,8,8,8,28,0,0,0,0,0},
[0x32]={0,28,34,2,4,8,16,62,0,0,0,0,0},[0x33]={0,62,4,8,4,2,34,28,0,0,0,0,0},
[0x34]={0,4,12,20,36,62,4,4,0,0,0,0,0},[0x35]={0,62,32,60,2,2,34,28,0,0,0,0,0},
[0x36]={0,12,16,32,60,34,34,28,0,0,0,0,0},[0x37]={0,62,2,4,8,16,16,16,0,0,0,0,0},
[0x38]={0,28,34,34,28,34,34,28,0,0,0,0,0},[0x39]={0,28,34,34,30,2,4,24,0,0,0,0,0},
[0x3a]={0,0,12,12,0,12,12,0,0,0,0,0,0},[0x3b]={0,0,12,12,0,12,8,16,0,0,0,0,0},
[0x3c]={0,2,4,8,16,8,4,2,0,0,0,0,0},[0x3d]={0,0,0,62,0,62,0,0,0,0,0,0,0},
[0x3e]={0,32,16,8,4,8,16,32,0,0,0,0,0},[0x3f]={0,28,34,2,4,8,0,8,0,0,0,0,0},
[0x40]={0,28,34,2,26,26,34,28,0,0,0,0,0},
[0x41]={0,8,20,34,34,62,34,34,0,0,0,0,0},[0x42]={0,60,34,34,60,34,34,60,0,0,0,0,0},
[0x43]={0,28,34,32,32,32,34,28,0,0,0,0,0},[0x44]={0,56,36,34,34,34,36,56,0,0,0,0,0},
[0x45]={0,62,32,32,60,32,32,62,0,0,0,0,0},[0x46]={0,62,32,32,60,32,32,32,0,0,0,0,0},
[0x47]={0,28,34,32,38,34,34,28,0,0,0,0,0},[0x48]={0,34,34,34,62,34,34,34,0,0,0,0,0},
[0x49]={0,28,8,8,8,8,8,28,0,0,0,0,0},[0x4a]={0,14,4,4,4,4,36,24,0,0,0,0,0},
[0x4b]={0,34,36,40,48,40,36,34,0,0,0,0,0},[0x4c]={0,32,32,32,32,32,32,62,0,0,0,0,0},
[0x4d]={0,34,54,42,42,34,34,34,0,0,0,0,0},[0x4e]={0,34,34,50,42,38,34,34,0,0,0,0,0},
[0x4f]={0,28,34,34,34,34,34,28,0,0,0,0,0},[0x50]={0,60,34,34,60,32,32,32,0,0,0,0,0},
[0x51]={0,28,34,34,34,42,36,26,0,0,0,0,0},[0x52]={0,60,34,34,60,40,36,34,0,0,0,0,0},
[0x53]={0,28,34,32,28,2,34,28,0,0,0,0,0},[0x54]={0,62,8,8,8,8,8,8,0,0,0,0,0},
[0x55]={0,34,34,34,34,34,34,28,0,0,0,0,0},[0x56]={0,34,34,34,34,20,20,8,0,0,0,0,0},
[0x57]={0,34,34,34,42,42,54,34,0,0,0,0,0},[0x58]={0,34,34,20,8,20,34,34,0,0,0,0,0},
[0x59]={0,34,34,20,8,8,8,8,0,0,0,0,0},[0x5a]={0,62,2,4,8,16,32,62,0,0,0,0,0},
[0x5b]={0,28,16,16,16,16,16,28,0,0,0,0,0},[0x5c]={0,64,32,16,8,4,2,1,0,0,0,0,0},
[0x5d]={0,28,4,4,4,4,4,28,0,0,0,0,0},[0x5e]={0,8,20,34,0,0,0,0,0,0,0,0,0},
[0x5f]={0,0,0,0,0,0,0,62,0,0,0,0,0},[0x60]={0,16,8,4,0,0,0,0,0,0,0,0,0},
[0x61]={0,0,0,28,2,30,34,30,0,0,0,0,0},[0x62]={0,32,32,60,34,34,34,60,0,0,0,0,0},
[0x63]={0,0,0,28,32,32,34,28,0,0,0,0,0},[0x64]={0,2,2,30,34,34,34,30,0,0,0,0,0},
[0x65]={0,0,0,28,34,62,32,28,0,0,0,0,0},[0x66]={0,12,18,16,60,16,16,16,0,0,0,0,0},
[0x67]={0,0,0,30,34,30,2,28,0,0,0,0,0},[0x68]={0,32,32,60,34,34,34,34,0,0,0,0,0},
[0x69]={0,8,0,24,8,8,8,28,0,0,0,0,0},[0x6a]={0,4,0,12,4,4,36,24,0,0,0,0,0},
[0x6b]={0,32,32,34,36,56,36,34,0,0,0,0,0},[0x6c]={0,24,8,8,8,8,8,28,0,0,0,0,0},
[0x6d]={0,0,0,52,42,42,42,34,0,0,0,0,0},[0x6e]={0,0,0,60,34,34,34,34,0,0,0,0,0},
[0x6f]={0,0,0,28,34,34,34,28,0,0,0,0,0},[0x70]={0,0,0,60,34,60,32,32,0,0,0,0,0},
[0x71]={0,0,0,30,34,30,2,2,0,0,0,0,0},[0x72]={0,0,0,44,48,32,32,32,0,0,0,0,0},
[0x73]={0,0,0,28,32,28,2,60,0,0,0,0,0},[0x74]={0,16,16,60,16,16,18,12,0,0,0,0,0},
[0x75]={0,0,0,34,34,34,34,30,0,0,0,0,0},[0x76]={0,0,0,34,34,34,20,8,0,0,0,0,0},
[0x77]={0,0,0,34,34,42,42,20,0,0,0,0,0},[0x78]={0,0,0,34,20,8,20,34,0,0,0,0,0},
[0x79]={0,0,0,34,34,30,2,28,0,0,0,0,0},[0x7a]={0,0,0,62,4,8,16,62,0,0,0,0,0},
[0x7b]={0,4,8,8,16,8,8,4,0,0,0,0,0},[0x7c]={0,8,8,8,8,8,8,8,0,0,0,0,0},
[0x7d]={0,16,8,8,4,8,8,16,0,0,0,0,0},[0x7e]={0,0,0,0,17,34,0,0,0,0,0,0,0},
/* Swedish characters: Å Ä Ö å ä ö */
[0xC5]={0,20,8,20,34,34,62,34,34,0,0,0,0}, /* Å */
[0xC4]={0,20,0,8,20,34,62,34,34,0,0,0,0}, /* Ä */
[0xD6]={0,20,0,28,34,34,34,34,28,0,0,0,0}, /* Ö */
[0xE5]={0,12,18,12,28,2,30,34,30,0,0,0,0}, /* å */
[0xE4]={0,20,0,28,2,30,34,30,0,0,0,0,0}, /* ä */
[0xF6]={0,20,0,28,34,34,34,28,0,0,0,0,0}, /* ö */
};

/* ============================== DRAW PRIMITIVES ============================== */
/* Shared slate / mint interface palette; saturated gameplay colours stay distinct. */
static uint32_t theme_color(uint32_t c) {
    switch(c) {
    case 0x0d0d1aff:case 0x0d0d18ff:case 0x0d0d1eff:case 0x0a0a14ff:return 0x10121fff;
    case 0x12122aff:case 0x10102aff:case 0x10101eff:case 0x14142cff:case 0x141420ff:case 0x16182aff:return 0x191c2fff;
    case 0x1a1a2eff:case 0x1a1a40ff:case 0x20203aff:case 0x181828ff:return 0x22263cff;
    case 0x2a2a5aff:case 0x1e1e46ff:case 0x1e1e3aff:return 0x303752ff;
    case 0x3a3a6aff:case 0x4a4a80ff:case 0x282840ff:case 0x1a1a34ff:return 0x3c4562ff;
    case 0x4040aaff:case 0x404060ff:case 0x4040a0ff:case 0x5050a0ff:case 0x6060a0ff:case 0x5050c0ff:case 0x383858ff:case 0x5050aaff:case 0x404040ff:return 0x8da0baff;
    case 0x22cc44ff:case 0x00cc44ff:return 0x72dfbbff;
    case 0xcc2222ff:return 0xf38b96ff;
    case 0x808080ff:return 0x9aacc3ff;
    case 0xc0c0c0ff:return 0xd4deecff;
    case 0xffffffff:return 0xf0f5fcff;
    default:return c;
    }
}
static void setc(uint32_t rgba) {
    rgba=theme_color(rgba);
    SDL_SetRenderDrawColor(g_rend,(rgba>>24)&255,(rgba>>16)&255,(rgba>>8)&255,rgba&255);
}
static void fr(int x,int y,int w,int h,uint32_t c){SDL_Rect r={x,y,w,h};setc(c);SDL_RenderFillRect(g_rend,&r);}
static void dr(int x,int y,int w,int h,uint32_t c){SDL_Rect r={x,y,w,h};setc(c);SDL_RenderDrawRect(g_rend,&r);}
static void round_fill(int x,int y,int w,int h,int radius,uint32_t color) {
    if(w<=0 || h<=0)return;
    if(radius>w/2)radius=w/2;if(radius>h/2)radius=h/2;
    setc(color);
    for(int row=0;row<h;row++) {
        int edge=0;
        if(row<radius){float dy=radius-row-.5f;edge=(int)ceilf(radius-sqrtf(radius*radius-dy*dy));}
        else if(row>=h-radius){float dy=row-(h-radius)+.5f;edge=(int)ceilf(radius-sqrtf(radius*radius-dy*dy));}
        SDL_RenderDrawLine(g_rend,x+edge,y+row,x+w-edge-1,y+row);
    }
}
static void surface(int x,int y,int w,int h,uint32_t fill,uint32_t border) {
    round_fill(x,y+2,w,h,9,0x080b14ff);
    round_fill(x,y,w,h,9,border);round_fill(x+1,y+1,w-2,h-2,8,fill);
}
/* One bright brick travels around the logo only while work is pending. */
static int logo_phase(Uint32 tick,int busy){return busy?(int)((tick/180)%3):1;}
static void brick_logo(int x,int y,int size,int gap,int busy) {
    int phase=logo_phase(SDL_GetTicks(),busy);
    const int dx[]={0,1,1},dy[]={0,0,1};
    for(int i=0;i<3;i++)round_fill(x+dx[i]*(size+gap),y+dy[i]*(size+gap),size,size,size/4,i==phase?0x8bedceff:0x285e56ff);
}
static unsigned int text_next(const char **text) {
    const unsigned char *s=(const unsigned char*)*text;unsigned int c=*s++;
    if(c>=0xc2 && c<=0xdf && (*s&0xc0)==0x80){c=((c&31)<<6)|(*s++&63);}
    else if(c>=0xe0 && c<=0xef && s[0] && s[1] && (s[0]&0xc0)==0x80 && (s[1]&0xc0)==0x80){c=((c&15)<<12)|((s[0]&63)<<6)|(s[1]&63);s+=2;}
    else if(c>=0xf0 && c<=0xf4 && s[0] && s[1] && s[2] && (s[0]&0xc0)==0x80 && (s[1]&0xc0)==0x80 && (s[2]&0xc0)==0x80){c=((c&7)<<18)|((s[0]&63)<<12)|((s[1]&63)<<6)|(s[2]&63);s+=3;}
    *text=(const char*)s;return c;
}
static int glyph_index(unsigned int cp) {
    if(cp<256)return (int)cp;
    for(unsigned int i=0;i<sizeof(UI_EXTRA)/sizeof(UI_EXTRA[0]);i++)if(UI_EXTRA[i]==cp)return 256+(int)i;
    return '?';
}
static void draw_glyph(float x,int y,unsigned int cp,uint32_t color,int sc) {
    int index=glyph_index(cp);const int *m=UI_GLYPHS[index];
    if(g_ui_font && m[2]>0 && m[3]>0) {
        color=theme_color(color);
        SDL_SetTextureColorMod(g_ui_font,color>>24,(color>>16)&255,(color>>8)&255);SDL_SetTextureAlphaMod(g_ui_font,color&255);
        SDL_Rect from={m[0],m[1],m[2],m[3]};
        /* Preserve fractional glyph bearings and dimensions: rounding every
         * glyph separately stretches strokes and makes spacing uneven. */
        SDL_FRect to={x+m[4]*sc/4.0f,y+12*sc+m[5]*sc/4.0f,m[2]*sc/4.0f,m[3]*sc/4.0f};
        SDL_RenderCopyF(g_rend,g_ui_font,&from,&to);
    } else if(!g_ui_font) {
        int ch=cp<256?(int)cp:'?';
        for(int row=0;row<FH;row++)for(int bit=0;bit<FW;bit++)if(FONT[ch][row]&(0x40>>bit))fr((int)x+bit*sc,y+row*sc,sc,sc,color);
    }
}
static int tw(const char *text,int sc) {
    float width=0;
    while(*text){unsigned int cp=text_next(&text);width+=UI_GLYPHS[glyph_index(cp)][6]*sc/256.0f;}
    return (int)lroundf(width);
}
static void dt(int x,int y,const char *text,uint32_t color,int sc) {
    if(g_high_contrast){unsigned brightness=((color>>24)&255)+((color>>16)&255)+((color>>8)&255);if(brightness>=160 && brightness<520)color=0xf0f5fcff;}
    float at=(float)x;
    while(*text){unsigned int cp=text_next(&text);draw_glyph(at,y,cp,color,sc);at+=UI_GLYPHS[glyph_index(cp)][6]*sc/256.0f;}
}
static void dtc(int cx,int y,const char *text,uint32_t color,int sc){dt(cx-tw(text,sc)/2,y,text,color,sc);}
static void dts(int x,int y,const char *text,uint32_t color,int sc){dt(x,y,text,color,sc);}
static int ui_font_init(void) {
    SDL_Surface *image=SDL_CreateRGBSurfaceWithFormat(0,UI_ATLAS_W,UI_ATLAS_H,32,SDL_PIXELFORMAT_RGBA32);
    if(!image)return 0;
    Uint32 *pixels=image->pixels;size_t at=0;
    for(size_t i=0;i<sizeof(UI_FONT_RLE);i+=2) {
        Uint32 pixel=SDL_MapRGBA(image->format,255,255,255,UI_FONT_RLE[i+1]);
        for(int n=0;n<UI_FONT_RLE[i] && at<UI_ATLAS_W*UI_ATLAS_H;n++)pixels[at++]=pixel;
    }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"1");g_ui_font=SDL_CreateTextureFromSurface(g_rend,image);SDL_FreeSurface(image);
    if(g_ui_font){
        SDL_SetTextureBlendMode(g_ui_font,SDL_BLENDMODE_BLEND);
#if SDL_VERSION_ATLEAST(2,0,12)
        SDL_SetTextureScaleMode(g_ui_font,SDL_ScaleModeLinear);
#endif
    }
    return g_ui_font!=NULL;
}

/* ============================== BUTTON SYSTEM ============================== */
static int pt_in(int px,int py,int rx,int ry,int rw,int rh){
    return px>=rx&&px<rx+rw&&py>=ry&&py<ry+rh;}

typedef enum{BS_NORMAL,BS_HOVER,BS_ACTIVE,BS_DISABLED,BS_TOGON}BtnStyle;

/* Returns 1 if clicked this frame */
static int dbtn(int x,int y,int w,int h,const char*lbl,BtnStyle sty){
    int hov=(!g_popup_blocked)&&pt_in(g_mouse.x,g_mouse.y,x,y,w,h);
    int clk=hov&&g_mouse.clicked&&(sty!=BS_DISABLED);
    if(clk) g_mouse.clicked=0; /* consume click so no other button fires */
    uint32_t bg,bo,tc;
    switch(sty) {
    case BS_DISABLED:bg=0x121d2bff;bo=0x243247ff;tc=0x6e8099ff;break;
    case BS_TOGON:bg=hov?0x24544aff:0x193d38ff;bo=0x327f6eff;tc=0x9af0d2ff;break;
    case BS_ACTIVE:bg=hov?0xd5c8ffff:0xbba5f4ff;bo=bg;tc=0x201b36ff;break;
    default:bg=hov?0x303951ff:0x20283bff;bo=hov?0x7383a5ff:0x39445eff;tc=0xdce6f3ff;
    }
    surface(x,y,w,h,bg,bo);
    dtc(x+w/2,y+(h-FH)/2,lbl,tc,1);
    return clk;
}
static int dtog(int x,int y,int w,int h,const char*loff,const char*lon,int on){
    return dbtn(x,y,w,h,on?lon:loff,on?BS_TOGON:BS_NORMAL);}

/* ============================== BLOCK RENDERING ============================== */
static const uint32_t PC[8]={0x0d0d1aff,0x67d8f0ff,0xe8ce70ff,0xc396eeff,0x77d8a1ff,0xf4838bff,0x829ef5ff,0xe9ab75ff};
/* Special type: 1=attack(red) 2=neutral(orange) 3=friendly(green)
 * Order matches SP_ defines: a c b r o q g s n i v m d f */
static const int SP_TYPE[NUM_SPECIALS+1]={
    0,
    1,  /* a AddLine     — attack  */
    3,  /* c ClearLine   — friendly */
    2,  /* b ClrSpecial  — neutral  */
    1,  /* r RandClear   — attack  */
    1,  /* o BlockBomb   — attack  */
    1,  /* q Quake       — attack  */
    1,  /* g Gravity     — attack  */
    1,  /* s Switch      — attack  */
    3,  /* n Nuke        — friendly */
    3,  /* i Immunity    — friendly */
    1,  /* v ClearColumn — attack  */
    1,  /* m Mutate      — attack  */
    1,  /* d Darkness    — attack  */
    1,  /* f Confusion   — attack  */
};
static const uint32_t SC[NUM_SPECIALS+1]={
    0,
    0xff4040ff, /* a AddLine     red       */
    0x40ff80ff, /* c ClearLine   green     */
    0xffcc40ff, /* b ClrSpecial  gold      */
    0xffaa40ff, /* r RandClear   orange    */
    0xff8040ff, /* o BlockBomb   orange-red*/
    0xff6060ff, /* q Quake       red       */
    0xff5050ff, /* g Gravity     red       */
    0xff8060ff, /* s Switch      salmon    */
    0x40ff40ff, /* n Nuke        bright gr */
    0x40ffccff, /* i Immunity    cyan-gr   */
    0xff4060ff, /* v ClearColumn crimson   */
    0xffaa60ff, /* m Mutate      peach     */
    0x8060ffff, /* d Darkness    purple    */
    0xff60a0ff, /* f Confusion   pink      */
};
static const char *SP_NAMES[NUM_SPECIALS+1]={
    "",
    "AddLine","ClearLine","ClrSpecial","RandClear","BlockBomb",
    "Quake","Gravity","Switch","Nuke","Immunity",
    "ClearColumn","Mutate","Darkness","Confusion",
};

static uint32_t ccol(uint8_t v){
    if(!v)                  return 0;
    if(v<=7)                return PC[v];
    if(v==8)                return 0x606060ff;
    if(v<=8+NUM_SPECIALS)   return SC[v-8];
    return 0x606060ff;
}

static void dblk(int px,int py,int sz,uint8_t val,int ghost){
    uint32_t col;
    if(!val){fr(px,py,sz,sz,0x0d0d1aff);dr(px,py,sz,sz,0x16162aff);return;}
    col=ccol(val);
    if(ghost){dr(px+1,py+1,sz-2,sz-2,(col&0xffffff00)|0x60);return;}
    /* Small gutters keep adjacent pieces legible without heavy bevels. */
    fr(px,py,sz,sz,0x101522ff);
    round_fill(px+1,py+1,sz-2,sz-2,sz>=16?3:1,col);
    {unsigned r=col>>24,g=(col>>16)&255,b=(col>>8)&255;
     uint32_t shine=((r+(255-r)/4)<<24)|((g+(255-g)/4)<<16)|((b+(255-b)/4)<<8)|255;
     fr(px+3,py+2,sz-6,1,shine);}
    if(g_patterns && val>=1 && val<=7){
        int dots=val,cell=sz>=18?3:1,spacing=cell+1,ox=px+3,oy=py+3;
        unsigned lum=(((col>>24)&255)*299+((col>>16)&255)*587+((col>>8)&255)*114)/1000;uint32_t marker=lum<140?0xf7faffff:0x101820ff;
        for(int i=0;i<dots;i++)fr(ox+(i%3)*spacing,oy+(i/3)*spacing,cell,cell,marker);
    }
    if(val>8&&val<=8+NUM_SPECIALS&&sz>=12){char s[2]={SPECIAL_CHARS[val-9],0};dts(px+sz/2-3,py+sz/2-5,s,0xffffffff,1);}
}

static void dfield(int ox,int oy,int sz,Field*f,Piece*cur,int show_ghost){
    int x,y;
    fr(ox,oy,FIELD_WIDTH*sz,FIELD_HEIGHT*sz,0x0d0d1aff);
    setc(0x16162aff);
    for(x=0;x<=FIELD_WIDTH;x++) SDL_RenderDrawLine(g_rend,ox+x*sz,oy,ox+x*sz,oy+FIELD_HEIGHT*sz);
    for(y=0;y<=FIELD_HEIGHT;y++) SDL_RenderDrawLine(g_rend,ox,oy+y*sz,ox+FIELD_WIDTH*sz,oy+y*sz);
    for(y=0;y<FIELD_HEIGHT;y++) for(x=0;x<FIELD_WIDTH;x++) if(f->cells[y][x]) dblk(ox+x*sz,oy+y*sz,sz,f->cells[y][x],0);
    if(cur&&show_ghost){
        Piece gh=*cur; const PieceShape*sh; int i;
        piece_drop(f,&gh); sh=&PIECES[gh.type][gh.rotation];
        for(i=0;i<4;i++){int gx=gh.x+sh->c[i].dx,gy=gh.y+sh->c[i].dy;
            if(gx>=0&&gx<FIELD_WIDTH&&gy>=0&&gy<FIELD_HEIGHT&&!f->cells[gy][gx])
                dblk(ox+gx*sz,oy+gy*sz,sz,(uint8_t)(cur->type+1),1);}
    }
    if(cur){
        const PieceShape*sh=&PIECES[cur->type][cur->rotation];int i;
        for(i=0;i<4;i++){int px=cur->x+sh->c[i].dx,py=cur->y+sh->c[i].dy;
            if(px>=0&&px<FIELD_WIDTH&&py>=0&&py<FIELD_HEIGHT) dblk(ox+px*sz,oy+py*sz,sz,(uint8_t)(cur->type+1),0);}
    }
}

static void dnext(int ox,int oy,int bw,int bh,Piece*p){
    const PieceShape*sh=&PIECES[p->type][0];int sz=bh<80?16:20,i,cx=ox+bw/2-sz,cy=oy+bh/2-sz;
    surface(ox,oy,bw,bh,0x171f30ff,0x303c54ff);dt(ox+8,oy+5,"NEXT",0xaebbd4ff,1);
    for(i=0;i<4;i++) dblk(cx+sh->c[i].dx*sz,cy+sh->c[i].dy*sz,sz,(uint8_t)(p->type+1),0);}

static void dhold(int ox,int oy,int bw,int bh,int ptype,int used,const char*keyhint){
    surface(ox,oy,bw,bh,used?0x121a27ff:0x171f30ff,0x303c54ff);
    dt(ox+4,oy+3,"HOLD",used?0x404040ff:COL_GRAY,1);
    dt(ox+4+tw("HOLD",1)+4,oy+3,keyhint,used?0x303030ff:0x505070ff,1);
    if(ptype>=0&&ptype<NUM_PIECES){
        const PieceShape*sh=&PIECES[ptype][0];int sz=bh<80?16:20,i,cx=ox+bw/2-sz,cy=oy+bh/2-sz;
        uint32_t col=used?0x303030ff:0xffffffff;
        (void)col;
        for(i=0;i<4;i++) dblk(cx+sh->c[i].dx*sz,cy+sh->c[i].dy*sz,sz,(uint8_t)(used?8:(ptype+1)),0);
    } else {
        dtc(ox+bw/2,oy+bh/2-FH/2,"—",0x303040ff,1);
    }
}

static void dpanel(int x,int y,int w,int h,const char*title){
    surface(x,y,w,h,0x181f31ff,0x303c54ff);
    if(title&&*title){dt(x+9,y+4,title,0x9caec6ff,1);}
}

/* ============================== SPECIALS BAR ==============================
 * Two sections:
 *   Row 1: SPECIALS queue (9 slots) + hover tooltip
 *   Taunt strip: 18px for taunt notifications
 * TARGETS removed — use number keys 1-8 mapped to display IDs.
 * ==================================================================== */
static BtnId draw_attack_bar(int ox,int oy,int bw,int bh,ClientCtx*c){
    BtnId action=BTN_NONE;
    int i,active=(c->state==CS_PLAYING&&!c->paused&&!c->is_spectator&&!c->no_attacks);

    const int TAUNT_H = 18;
    const int ROW_H   = bh - TAUNT_H;
    const int PAD     = 4;
    const int SLOT_SZ = 32;
    const int SLOT_GAP= 3;
    const int NUM_SL  = 9;
    const int LBL_W   = 72;

    fr(ox,oy,bw,bh,0x0a0a14ff);
    dr(ox,oy,bw,bh,COL_BORDER);

    /* -------- SPECIALS ROW -------- */
    {
        int r1y   = oy + 1;
        int sloth = ROW_H - 2;
        int slot_y= r1y + (sloth - SLOT_SZ)/2;
        int slots_end_x = ox+PAD+LBL_W + NUM_SL*(SLOT_SZ+SLOT_GAP);
        dt(ox+PAD, r1y+(sloth-FH)/2, "SPECIALS", 0x4a4a78ff, 1);
        int hovered_sp = 0;
        for(i=0;i<NUM_SL;i++){
            int sx = ox+PAD+LBL_W + i*(SLOT_SZ+SLOT_GAP);
            uint8_t sp = (i<c->my_field.special_count)
                       ? c->my_field.specials[(c->my_field.special_head+i)%64] : 0;
            uint32_t col = sp ? SC[sp<=NUM_SPECIALS?sp:0] : 0x0e0e1eff;
            fr(sx,slot_y,SLOT_SZ,SLOT_SZ, sp?col:0x111120ff);
            if(i==0&&sp) dr(sx-1,slot_y-1,SLOT_SZ+2,SLOT_SZ+2,0xffffffff);
            else         dr(sx,slot_y,SLOT_SZ,SLOT_SZ,sp?0x505078ff:0x1e1e30ff);
            if(sp&&sp<=NUM_SPECIALS){
                int spt=SP_TYPE[sp];
                uint32_t type_col = spt==1?0xff3030ff:spt==3?0x30ff60ff:0xffaa20ff;
                char s[2]={SPECIAL_CHARS[sp-1],'\0'};
                dts(sx+SLOT_SZ/2-3,slot_y+SLOT_SZ/2-6,s,0xffffffff,1);
                fr(sx+2,slot_y+2,6,6,type_col);
                if(active&&pt_in(g_mouse.x,g_mouse.y,sx,slot_y,SLOT_SZ,SLOT_SZ)){
                    dr(sx,slot_y,SLOT_SZ,SLOT_SZ,type_col);
                    hovered_sp = sp;
                }
            }
        }
        /* Hover tooltip right of slots — show first special by default,
           switch to hovered slot when mouse is over one */
        {
            int tip_sp = 0;
            /* Default: first special in queue */
            if (c->my_field.special_count > 0)
                tip_sp = c->my_field.specials[c->my_field.special_head % 64];
            /* Override with hovered slot if any */
            if (hovered_sp > 0)
                tip_sp = hovered_sp;
            if (tip_sp > 0 && tip_sp <= NUM_SPECIALS) {
                int spt = SP_TYPE[tip_sp];
                char tip[64];
                snprintf(tip, sizeof(tip), "%s%s [%s]",
                    hovered_sp ? "" : "\x10 ",   /* ► prefix when showing default */
                    SP_NAMES[tip_sp],
                    spt==1?"ATTACK":spt==3?"FRIENDLY":"NEUTRAL");
                uint32_t tc = spt==1?0xff8080ff:spt==3?0x80ff80ff:0xffcc60ff;
                /* Dim slightly when showing default (not hovered) */
                if (!hovered_sp) tc = (tc & 0xffffff00) | 0xa0;
                dt(slots_end_x+6, slot_y+SLOT_SZ/2-FH/2, tip, tc, 1);
            }
        }
    }

    /* -------- TAUNT STRIP -------- */
    {
        int ty = oy + ROW_H;
        fr(ox,ty,bw,TAUNT_H,0x100a00ff);
        dr(ox,ty,bw,TAUNT_H,0x2a2000ff);
        if(c->taunt_timer>0){
            uint8_t alpha=(c->taunt_timer>60)?255:(uint8_t)(c->taunt_timer*255/60);
            uint32_t col=(0xffcc5500)|(alpha);
            dtc(ox+bw/2, ty+(TAUNT_H-FH)/2, c->taunt_text, col, 1);
        } else {
            dtc(ox+bw/2, ty+(TAUNT_H-FH)/2, "-- no taunts yet --", 0x222018ff, 1);
        }
    }
    return action;
}
static void draw_activity_log(int ox, int oy, int w, int h, ClientCtx *c) {
    int lh = FH+3, i;
    int maxl = (h - 20) / lh;
    int start = c->act_count - maxl; if(start<0) start=0;
    /* Same style as chat panel: dark background, border, plain text label */
    surface(ox,oy,w,h,0x141c2cff,0x35415bff);
    dt(ox+4,oy+3,"ACTIVITY",COL_GRAY,1);
    for(i=start;i<c->act_count;i++){
        int ty=oy+20+(i-start)*lh;
        uint32_t col=c->act_log[i].colour;
        if(c->act_log[i].age>180){
            int fade=255-(c->act_log[i].age-180)*255/120;
            if(fade<40) fade=40;
            col=(col&0xffffff00)|(uint8_t)fade;
        }
        char cl[MAX_ACT_LEN]; int mc=(w-8)/(FW+1);
        if(mc>=(int)sizeof(cl)) mc=(int)sizeof(cl)-1;
        strncpy(cl,c->act_log[i].text,mc); cl[mc]='\0';
        dt(ox+4,ty,cl,col,1);
    }
    if(c->act_count==0)
        dt(ox+4,oy+20,"(no specials yet)",0x303050ff,1);
}

/* ============================== OPPONENT MINI FIELD ============================== */
static void dopp(int ox,int oy,Field*f,const char*nick,int alive,int spectating,int slot){
    int sz=MINI_BLOCK,fw=FIELD_WIDTH*sz,fh=FIELD_HEIGHT*sz,x,y;
    /* Panel border — purple tint for spectators */
    uint32_t border_col = spectating ? 0x6060a0ff : COL_BORDER;
    fr(ox-2,oy-16,fw+4,fh+20,spectating?0x0a0a22ff:COL_HIGHLIGHT);
    dr(ox-2,oy-16,fw+4,fh+20,border_col);

    /* Header: slot chip left, nick centre, SPEC badge right */
    if(slot>0){
        char s[4]; snprintf(s,sizeof(s),"%d",slot);
        fr(ox-2,oy-16,FW+6,16,spectating?0x101022ff:0x1a1a00ff);
        dt(ox,oy-14,s,spectating?0x6060a0ff:COL_HOSTGOLD,1);
    }
    int nick_x = ox + (slot>0 ? FW+5 : 0);
    dt(nick_x,oy-14,nick?nick:"?",spectating?0x5050a0ff:alive?COL_LTGRAY:0x505050ff,1);
    /* Spectator badge — right side of header, clear purple label */
    if(spectating){
        int bw=tw("SPEC",1)+6;
        fr(ox+fw-bw-2,oy-16,bw+2,16,0x20204aff);
        dt(ox+fw-bw,oy-14,"SPEC",0x8080ccff,1);
    }

    /* Field background */
    fr(ox,oy,fw,fh,spectating?0x080814ff:0x0d0d1aff);
    for(y=0;y<FIELD_HEIGHT;y++) for(x=0;x<FIELD_WIDTH;x++){
        uint8_t v=f->cells[y][x]; if(v) dblk(ox+x*sz,oy+y*sz,sz-1,v,0);}

    /* Spectator overlay — dim the field with SPECTATOR text */
    if(spectating){
        SDL_SetRenderDrawBlendMode(g_rend,SDL_BLENDMODE_BLEND);
        fr(ox,oy,fw,fh,0x00000088);
        SDL_SetRenderDrawBlendMode(g_rend,SDL_BLENDMODE_NONE);
        dtc(ox+fw/2,oy+fh/2-FH,"SPECTATOR",0x6060a0ff,1);
        dtc(ox+fw/2,oy+fh/2+2,"(watching)",0x404080ff,1);
        return;
    }

    if(!alive){
        SDL_SetRenderDrawBlendMode(g_rend,SDL_BLENDMODE_BLEND);
        fr(ox,oy,fw,fh,0x00000099);
        SDL_SetRenderDrawBlendMode(g_rend,SDL_BLENDMODE_NONE);
        dtc(ox+fw/2,oy+fh/2-6,"OUT",COL_INGAME_RED,1);}
}

/* ============================== CHAT PANEL ============================== */
static int draw_chat_panel(int ox,int oy,int w,int h,ClientCtx*c){
    int line_h=FH+3,i,clicked_chat=0;
    int input_h=22, hdr_h=20, sep=1, sb_w=8;
    int msg_w=w-sb_w; /* leave room for scrollbar */
    int msg_h=h-hdr_h-sep-sep-input_h;
    int max_l=msg_h/line_h; if(max_l<1)max_l=1;

    /* Clamp scroll: 0=bottom, max scroll = max history - visible */
    int max_scroll=c->chat_count-max_l; if(max_scroll<0)max_scroll=0;
    if(c->chat_scroll>max_scroll) c->chat_scroll=max_scroll;
    if(c->chat_scroll<0)          c->chat_scroll=0;

    /* Panel background + border */
    surface(ox,oy,w,h,0x141c2cff,0x35415bff);

    /* Header */
    dt(ox+4,oy+4,"ROOM CHAT",0x6060a0ff,1);
    {int btn_w=tw("Chat (Tab)",1)+14;
     if(dbtn(ox+w-sb_w-btn_w-2,oy+2,btn_w,16,"Chat (Tab)",BS_NORMAL)) clicked_chat=1;}
    fr(ox+8,oy+hdr_h,w-16,sep,0x303c54ff);

    /* Messages — scroll: 0=show newest, N=show N lines back */
    {int end=c->chat_count - c->chat_scroll; if(end<0)end=0;
     int start=end-max_l; if(start<0)start=0;
     for(i=start;i<end;i++){
         int ty=oy+hdr_h+sep+2+(i-start)*line_h;
         uint32_t col=c->chat[i].colour;
         if(c->chat[i].age>300)col=(col&0xffffff00)|0x80;
         char cl[120]; int mc=(msg_w-8)/(FW+1); if(mc>119)mc=119;
         strncpy(cl,c->chat[i].text,mc);cl[mc]=0;
         dt(ox+4,ty,cl,col,1);
     }}

    /* Scrollbar */
    {int sty=oy+hdr_h+sep, sth=h-hdr_h-sep-sep-input_h;
     int sbx=ox+msg_w;
     fr(sbx,sty,sb_w,sth,0x08081aff);
     if(max_scroll>0){
         int thumb_h=sth*max_l/(c->chat_count>0?c->chat_count:1);
         if(thumb_h<12)thumb_h=12;
         int thumb_y=sty+(sth-thumb_h)*(max_scroll-c->chat_scroll)/max_scroll;
         /* Highlight thumb when dragging */
         uint32_t thumb_col=c->chat_sb_drag?0x6060b0ff:0x3a3a70ff;
         fr(sbx+1,thumb_y,sb_w-2,thumb_h,thumb_col);
         /* Start drag on mousedown over thumb */
         if(g_mouse.held&&pt_in(g_mouse.x,g_mouse.y,sbx+1,thumb_y,sb_w-2,thumb_h)
            &&!c->chat_sb_drag){
             c->chat_sb_drag=1;
             c->chat_sb_drag_y0=g_mouse.y;
             c->chat_sb_drag_scroll0=c->chat_scroll;
         }
         /* Continue drag */
         if(c->chat_sb_drag&&g_mouse.held){
             int dy=g_mouse.y-c->chat_sb_drag_y0;
             /* dy pixels maps to scroll: negative dy = scroll up (more history) */
             int ds=dy*(max_scroll+1)/(sth-thumb_h+1);
             c->chat_scroll=c->chat_sb_drag_scroll0-ds;
             if(c->chat_scroll<0)c->chat_scroll=0;
             if(c->chat_scroll>max_scroll)c->chat_scroll=max_scroll;
         }
         /* Release drag */
         if(!g_mouse.held) c->chat_sb_drag=0;
         /* Click track (outside thumb) to jump */
         if(pt_in(g_mouse.x,g_mouse.y,sbx,sty,sb_w,sth)&&g_mouse.clicked
            &&!pt_in(g_mouse.x,g_mouse.y,sbx+1,thumb_y,sb_w-2,thumb_h)){
             int rel=g_mouse.y-sty;
             c->chat_scroll=max_scroll-(rel*max_scroll/sth);
         }
     } else {
         if(!g_mouse.held) c->chat_sb_drag=0;
     }}

    /* Separator + input box */
    fr(ox,oy+h-input_h-sep,w,sep,0x3a3a70ff);
    {int iy=oy+h-input_h;
     if(c->chat_input.active){
         fr(ox,iy,w,input_h,0x14143aff);
         dr(ox,iy,w,input_h,0xaaaaffff);
         char d[160]; snprintf(d,sizeof(d),"> %s_",c->chat_input.text);
         dt(ox+6,iy+(input_h-FH)/2,d,0xeeeeffff,1);
     } else {
         fr(ox,iy,w,input_h,0x08081aff);
         dr(ox,iy,w,input_h,0x2a2a50ff);
         dt(ox+6,iy+(input_h-FH)/2,"Tab to chat...",0x404070ff,1);
     }}
    return clicked_chat;
}

/* ============================== WINNER BANNER ============================== */
static void draw_winner(ClientCtx*c){
    if(!c->winner[0]||c->winner_timer<=0)return;
    char msg[64];snprintf(msg,sizeof(msg),"  %s WINS!  ",c->winner);
    int bw=tw(msg,2)+20,bh=FH*2+20,bx=WIN_W/2-bw/2,by=WIN_H/2-bh/2-40;
    int al=(c->winner_timer>60)?230:(c->winner_timer*230/60);
    SDL_SetRenderDrawBlendMode(g_rend,SDL_BLENDMODE_BLEND);
    fr(bx-4,by-4,bw+8,bh+8,al);fr(bx,by,bw,bh,(0x1a1a2e00)|al);
    dr(bx,by,bw,bh,COL_HOSTGOLD);
    SDL_SetRenderDrawBlendMode(g_rend,SDL_BLENDMODE_NONE);
    dts(bx+10,by+10,msg,COL_HOSTGOLD,2);}

/* ============================== TEXT FIELD ============================== */
static int draw_textfield(int x,int y,int w,int h,TextField*tf,const char*placeholder){
    int hov=pt_in(g_mouse.x,g_mouse.y,x,y,w,h);
    int clicked=(hov&&g_mouse.clicked);
    uint32_t bg=tf->focused?0x1e1e3aff:0x141420ff;
    uint32_t bo=tf->focused?0x8080c0ff:0x3a3a6aff;
    fr(x,y,w,h,bg);dr(x,y,w,h,bo);
    if(tf->len>0){
        char disp[130];snprintf(disp,sizeof(disp),"%s%s",tf->text,tf->focused?"_":"");
        dt(x+5,y+(h-FH)/2,disp,COL_WHITE,1);
    } else {
        dt(x+5,y+(h-FH)/2,placeholder,0x404060ff,1);
        if(tf->focused) dt(x+5,y+(h-FH)/2,"_",0x8080a0ff,1);
    }
    return clicked;
}

/* ============================== CONNECT SCREEN ============================== */
/* Draw a password field — same as textfield but shows asterisks */
static int draw_passfield(int x,int y,int w,int h,TextField*tf,const char*placeholder){
    int hov=pt_in(g_mouse.x,g_mouse.y,x,y,w,h);
    int clicked=(hov&&g_mouse.clicked);
    uint32_t bg=tf->focused?0x1e1e3aff:0x141420ff;
    uint32_t bo=tf->focused?0x8080c0ff:0x3a3a6aff;
    char stars[130];
    int i;
    fr(x,y,w,h,bg); dr(x,y,w,h,bo);
    if(tf->len>0){
        for(i=0;i<tf->len&&i<128;i++) stars[i]='*';
        if(tf->focused) stars[i++]='_';
        stars[i]='\0';
        dt(x+5,y+(h-FH)/2,stars,COL_WHITE,1);
    } else {
        dt(x+5,y+(h-FH)/2,placeholder,0x404060ff,1);
        if(tf->focused) dt(x+5,y+(h-FH)/2,"_",0x8080a0ff,1);
    }
    return clicked;
}

/* ============================== SERVER LIST ============================== */
/* ── Shared scancode -> human-readable name ── */
static const char *sc_name(int sc) {
    switch(sc) {
        case 4:  return "A";       case 5:  return "B";
        case 6:  return "C";       case 7:  return "D";
        case 8:  return "E";       case 9:  return "F";
        case 10: return "G";       case 11: return "H";
        case 12: return "I";       case 13: return "J";
        case 14: return "K";       case 15: return "L";
        case 16: return "M";       case 17: return "N";
        case 18: return "O";       case 19: return "P";
        case 20: return "Q";       case 21: return "R";
        case 22: return "S";       case 23: return "T";
        case 24: return "U";       case 25: return "V";
        case 26: return "W";       case 27: return "X";
        case 28: return "Y";       case 29: return "Z";
        case 30: return "1";       case 31: return "2";
        case 32: return "3";       case 33: return "4";
        case 34: return "5";       case 35: return "6";
        case 36: return "7";       case 37: return "8";
        case 38: return "9";       case 39: return "0";
        case 40: return "Enter";   case 41: return "Escape";
        case 42: return "Bksp";    case 43: return "Tab";
        case 44: return "Space";   case 45: return "-";
        case 46: return "=";       case 58: return "F1";
        case 59: return "F2";      case 60: return "F3";
        case 61: return "F4";      case 62: return "F5";
        case 63: return "F6";      case 64: return "F7";
        case 65: return "F8";      case 79: return "Right";
        case 80: return "Left";    case 81: return "Down";
        case 82: return "Up";      case 224: return "LCtrl";
        case 225: return "LShift"; case 226: return "LAlt";
        case 228: return "RCtrl";  case 229: return "RShift";
        case 230: return "RAlt";
        default: { static char buf[8]; snprintf(buf,sizeof(buf),"#%d",sc); return buf; }
    }
}

/* ── Shared keymap popup render — call from any screen ── */
static void render_keymap_popup(ClientCtx *c, int saved_click) {
    static const char *KM_LABELS[] = {
        "Move Left","Move Right","Rotate CW","Rotate CCW",
        "Soft Drop","Hard Drop","Hold","Hold (alt)",
        "Chat","Pause","Ready (toggle)"
    };
    int *km_fields[] = {
        &c->km.move_left, &c->km.move_right,
        &c->km.rotate_cw, &c->km.rotate_ccw,
        &c->km.soft_drop, &c->km.hard_drop,
        &c->km.hold,      &c->km.hold2,
        &c->km.chat,      &c->km.pause,
        &c->km.ready
    };
    const int NKEYS = 11;
    const int ROW_H = 30, ROW_PAD = 4;
    int pw=500, ph=560;
    int px=(WIN_W-pw)/2, py=(WIN_H-ph)/2;
    int capturing = (c->keymap_capture_idx >= 0);

    SDL_SetRenderDrawBlendMode(g_rend,SDL_BLENDMODE_BLEND);
    fr(0,0,WIN_W,WIN_H,0x00000099);
    SDL_SetRenderDrawBlendMode(g_rend,SDL_BLENDMODE_NONE);

    /* saved_click is the real this-frame click captured before the caller
     * zeroed g_mouse.clicked for popup blocking. Use it for outside-click
     * guard AND restore it into g_mouse.clicked for all internal widgets. */
    if(!capturing && saved_click && !pt_in(g_mouse.x,g_mouse.y,px,py,pw,ph)){
        c->keymap_popup_open=0; c->keymap_capture_idx=-1; g_mouse.clicked=0; return;
    }

    /* Restore real click into g_mouse.clicked so dbtn and row hits work */
    g_popup_blocked = 0;
    g_mouse.clicked = saved_click;

    surface(px,py,pw,ph,0x181f31ff,0x55637fff);
    dt(px+20,py+16,"Settings",COL_WHITE,2);
    dt(px+20,py+47,"Preferences for this client",COL_GRAY,1);
    const char *tabs[]={"Display","Controls","Audio"};
    for(int tab=0;tab<3;tab++)if(dbtn(px+20+tab*156,py+76,148,30,tabs[tab],c->settings_tab==tab?BS_ACTIVE:BS_NORMAL)&&!capturing)c->settings_tab=tab;
    if(c->settings_tab==1) {

    for(int i=0;i<NKEYS;i++){
        int bx=px+20, by=py+120+i*(ROW_H+ROW_PAD);
        int bw=pw-24, bh=ROW_H;
        int is_cap=(c->keymap_capture_idx==i);
        int hv=(!capturing||is_cap)&&pt_in(g_mouse.x,g_mouse.y,bx,by,bw,bh);
        uint32_t bg=is_cap?0x201840ff:hv?0x141428ff:0x0c0c1eff;
        uint32_t border=is_cap?0x9060ffff:hv?0x4040a0ff:0x222244ff;
        fr(bx,by,bw,bh,bg); dr(bx,by,bw,bh,border);
        dt(bx+10,by+bh/2-FH/2,KM_LABELS[i],is_cap?0xc0a0ffff:0x8080a0ff,1);
        if(is_cap){
            const char *prompt="Press any key...";
            uint32_t pc=(int)(SDL_GetTicks64()/500)%2?0x9060ffff:0xc080ffff;
            dt(bx+bw-tw(prompt,1)-10,by+bh/2-FH/2,prompt,pc,1);
        } else {
            const char *nm=sc_name(*km_fields[i]);
            dt(bx+bw-tw(nm,1)-10,by+bh/2-FH/2,nm,0x8080c0ff,1);
        }
        if(!is_cap && hv && g_mouse.clicked){
            c->keymap_capture_idx=i; g_mouse.clicked=0;
        }
    }
    }
    if(c->settings_tab==0){
        char label[64];
        dt(px+20,py+132,"Window size",COL_WHITE,1);
        snprintf(label,sizeof(label),"%d%%",c->ui_scale?c->ui_scale:100);
        if(dbtn(px+310,py+122,170,34,label,BS_NORMAL))client_cycle_ui_scale(c);
        dt(px+20,py+190,"Piece patterns",COL_WHITE,1);
        if(dtog(px+310,py+180,170,34,"Off","On",c->piece_patterns)){c->piece_patterns=!c->piece_patterns;sdl_set_ui_options(c->ui_scale,c->piece_patterns,c->high_contrast);client_save_settings(c);}
        dt(px+20,py+248,"High contrast text",COL_WHITE,1);
        if(dtog(px+310,py+238,170,34,"Off","On",c->high_contrast)){c->high_contrast=!c->high_contrast;sdl_set_ui_options(c->ui_scale,c->piece_patterns,c->high_contrast);client_save_settings(c);}
        dt(px+20,py+306,"Controller input",COL_WHITE,1);
        if(dtog(px+310,py+296,170,34,"Off","On",c->controller_enabled)){c->controller_enabled=!c->controller_enabled;client_save_settings(c);}
        dt(px+20,py+358,g_controller?SDL_GameControllerName(g_controller):"Connect an Xbox or PlayStation controller.",COL_GRAY,1);
        dt(px+20,py+390,"A / Cross: rotate   B / Circle: reverse",COL_GRAY,1);
        dt(px+20,py+416,"X / Square: hold   Y / Triangle: hard drop",COL_GRAY,1);

    }
    if(c->settings_tab==2){
        dt(px+20,py+132,"Music",COL_WHITE,1);
        if(dtog(px+310,py+122,170,34,"Off","On",music_get_enabled())){music_set_enabled(!music_get_enabled());client_save_settings(c);}
        dt(px+20,py+190,"Sound effects",COL_WHITE,1);
        if(dtog(px+310,py+180,170,34,"Off","On",c->sound_enabled)){c->sound_enabled=!c->sound_enabled;client_save_settings(c);}
        for(int audio=0;audio<2;audio++){
            int y=py+248+audio*58;float value=audio?sound_get_sfx_volume():music_get_volume();char label[24];
            dt(px+20,y+10,audio?"Effects volume":"Music volume",COL_WHITE,1);
            snprintf(label,sizeof(label),"%d%%",(int)lroundf(value*100));dtc(px+394,y+10,label,COL_LTGRAY,1);
            int change=0;
            if(dbtn(px+310,y,42,34,"-",BS_NORMAL))change=-1;
            if(dbtn(px+438,y,42,34,"+",BS_NORMAL))change=1;
            if(change){value+=change*.1f;if(value<0)value=0;if(value>1)value=1;if(audio)sound_set_sfx_volume(value);else music_set_volume(value);client_save_settings(c);}
        }
        dt(px+20,py+392,"Music selection is available during a game.",COL_GRAY,1);
    }
    if(dbtn(px+12,py+ph-44,pw-24,32,"Close",BS_NORMAL)){
        c->keymap_popup_open=0; c->keymap_capture_idx=-1;
    }
    /* Consume any remaining click so background buttons don't fire after popup */
    g_mouse.clicked = 0;
}

/* Music controls are client-local and never send gameplay commands. */
static void music_link(const char *url) {
#if SDL_VERSION_ATLEAST(2,0,14)
    SDL_OpenURL(url);
#elif defined(PLATFORM_WINDOWS)
    ShellExecuteA(NULL,"open",url,NULL,NULL,SW_SHOWNORMAL);
#else
    SDL_SetClipboardText(url); /* Older SDL: make the attribution URL available. */
#endif
}
static void music_time(double seconds,char out[24]) {
    int total=(int)(seconds>0?seconds:0);
    snprintf(out,24,"%d:%02d",total/60,total%60);
}
static void render_music_popup(ClientCtx *c,int saved_click) {
    int pw=780,ph=654,px=(WIN_W-pw)/2,py=(WIN_H-ph)/2;
    MusicPlayback p; music_snapshot(&p);
    const MusicTrack *track=music_track(p.index);
    SDL_SetRenderDrawBlendMode(g_rend,SDL_BLENDMODE_BLEND); fr(0,0,WIN_W,WIN_H,0x000000b0);
    SDL_SetRenderDrawBlendMode(g_rend,SDL_BLENDMODE_NONE);
    if(saved_click && !pt_in(g_mouse.x,g_mouse.y,px,py,pw,ph)) {
        c->music_player_open=0; g_mouse.clicked=0; return;
    }
    g_popup_blocked=0; g_mouse.clicked=saved_click;
    fr(px,py,pw,ph,0x10102aff); dr(px,py,pw,ph,COL_PIECE_I);
    fr(px+1,py+1,pw-2,32,0x1a1a40ff); dt(px+20,py+10,"MUSIC PLAYER",COL_PIECE_I,1);
    dt(px+pw-268,py+10,"Local audio / Esc to close",COL_GRAY,1);
    dt(px+22,py+48,p.index?"NOW PLAYING / GAME LIBRARY":"NOW PLAYING / TITLE THEME",COL_GRAY,1);
    dt(px+22,py+68,track->title,COL_LTGRAY,2);
    {char line[160]; snprintf(line,sizeof(line),"%s  /  %s",track->artist,track->license);
     dt(px+22,py+100,line,COL_OPEN_GREEN,1);}
    int sx=px+22,sy=py+124,sw=pw-44;
    fr(sx,sy,sw,8,0x252540ff);
    if(p.duration>0) {
        int fill=(int)(sw*p.position/p.duration); if(fill>sw)fill=sw;
        fr(sx,sy,fill,8,COL_PIECE_I);
        if(g_mouse.clicked && pt_in(g_mouse.x,g_mouse.y,sx,sy-5,sw,18)) {
            music_seek(p.duration*(g_mouse.x-sx)/sw); g_mouse.clicked=0;
        }
    }
    {char pos[24],length[24],label[64]; music_time(p.position,pos); music_time(p.duration,length);
     snprintf(label,sizeof(label),"%s / %s",pos,length); dt(sx,py+140,label,COL_GRAY,1);}
    const char *status=!p.available?"No audio device":p.failed?"Track unavailable":!p.enabled?"Music disabled":p.loading?"Loading...":p.paused?"Paused":"Playing";
    dt(px+pw-22-tw(status,1),py+140,status,p.failed?COL_INGAME_RED:COL_GRAY,1);
    int y=py+166;
    if(dbtn(px+22,y,96,30,"Previous",BS_NORMAL)) music_previous();
    const char *play=(!p.enabled||p.paused)?"Play":"Pause";
    if(dbtn(px+126,y,96,30,play,BS_ACTIVE)) {
        if(!p.enabled) { music_set_enabled(1); if(p.paused) music_toggle_pause(); }
        else music_toggle_pause();
        client_save_settings(c);
    }
    if(dbtn(px+230,y,96,30,"Next",BS_NORMAL)) music_next();
    if(dbtn(px+338,y,110,30,p.shuffle?"Shuffle ON":"Shuffle OFF",p.shuffle?BS_TOGON:BS_NORMAL)) {
        music_set_shuffle(!p.shuffle); client_save_settings(c);
    }
    if(dbtn(px+474,y,36,30,"-",BS_NORMAL)) { music_set_volume(music_get_volume()-0.05f); client_save_settings(c); }
    {char label[32]; snprintf(label,sizeof(label),"Volume %d%%",(int)(music_get_volume()*100+0.5f)); dtc(px+578,y+9,label,COL_LTGRAY,1);}
    if(dbtn(px+650,y,36,30,"+",BS_NORMAL)) { music_set_volume(music_get_volume()+0.05f); client_save_settings(c); }
    int count=music_track_count()-1, visible=5, max_scroll=count>visible?count-visible:0;
    if(c->music_last_index!=p.index) {
        if(p.index>0 && (p.index-1<c->music_scroll || p.index-1>=c->music_scroll+visible)) c->music_scroll=p.index-1;
        c->music_last_index=p.index;
    }
    if(c->music_scroll<0)c->music_scroll=0; if(c->music_scroll>max_scroll)c->music_scroll=max_scroll;
    dt(px+22,py+216,"GAME SOUNDTRACK / select a track to play",COL_GRAY,1);
    {char range[32]; snprintf(range,sizeof(range),"%d-%d / %d",c->music_scroll+1,
        c->music_scroll+visible<count?c->music_scroll+visible:count,count); dt(px+pw-266,py+216,range,COL_GRAY,1);}
    if(dbtn(px+pw-182,py+208,66,24,"Up",c->music_scroll?BS_NORMAL:BS_DISABLED)) c->music_scroll-=visible;
    if(dbtn(px+pw-108,py+208,86,24,"Down",c->music_scroll<max_scroll?BS_NORMAL:BS_DISABLED)) c->music_scroll+=visible;
    if(c->music_scroll<0)c->music_scroll=0; if(c->music_scroll>max_scroll)c->music_scroll=max_scroll;
    for(int row_index=0;row_index<visible && row_index+c->music_scroll<count;row_index++) {
        int i=1+c->music_scroll+row_index;
        const MusicTrack *item=music_track(i); int row=py+238+row_index*46;
        int selected=p.index==i;
        fr(px+22,row,pw-44,42,selected?0x203952ff:0x16182cff);
        dr(px+22,row,pw-44,42,selected?COL_PIECE_I:COL_BORDER);
        if(g_mouse.clicked && pt_in(g_mouse.x,g_mouse.y,px+22,row,pw-44,42)) {
            music_select(i); client_save_settings(c); g_mouse.clicked=0;
        }
        dt(px+32,row+7,selected?">":" ",selected?COL_PIECE_I:COL_GRAY,1);
        dt(px+52,row+6,item->title,selected?COL_PIECE_I:COL_LTGRAY,1);
        dt(px+52,row+24,item->artist,COL_GRAY,1);
        dt(px+pw-40-tw(item->license,1),row+15,item->license,COL_OPEN_GREEN,1);
    }
    if(max_scroll) {
        int h=230*visible/count, y=py+238+(230-h)*c->music_scroll/max_scroll;
        fr(px+pw-30,py+238,5,230,COL_BORDER); fr(px+pw-30,y,5,h,COL_PIECE_I);
    }
    int cy=py+478;
    dt(px+22,cy,"CURRENT TRACK CREDITS",COL_GRAY,1); cy+=19;
    dt(px+22,cy,track->source_url,COL_OPEN_GREEN,1);
    if(g_mouse.clicked && pt_in(g_mouse.x,g_mouse.y,px+22,cy,pw-44,16)) { music_link(track->source_url); g_mouse.clicked=0; }
    cy+=18; dt(px+22,cy,track->license_url,COL_OPEN_GREEN,1);
    if(g_mouse.clicked && pt_in(g_mouse.x,g_mouse.y,px+22,cy,pw-44,16)) { music_link(track->license_url); g_mouse.clicked=0; }
    cy+=18; dt(px+22,cy,track->attribution,COL_GRAY,1);
    dt(px+22,py+560,"Title theme: Sci-fi Theme / MAOU (maou) / CC-BY 4.0",COL_LTGRAY,1);
    dt(px+22,py+579,"Audio files unmodified. Complete attributions: CREDITS.md",COL_GRAY,1);
    if(dbtn(px+pw-138,py+ph-43,116,28,"Close",BS_NORMAL)) c->music_player_open=0;
    g_mouse.clicked=0;
}

/* ── Shared about popup render ── */
void sdl_open_account_code(const char *code){char url[128];snprintf(url,sizeof(url),"https://lineclash.kladhest.se/account/%s%s",code[0]?"?code=":"",code);music_link(url);}

static void render_about_popup(ClientCtx *c, int saved_click) {
    int pw=640, ph=540, px=(WIN_W-pw)/2, py=(WIN_H-ph)/2;

    SDL_SetRenderDrawBlendMode(g_rend,SDL_BLENDMODE_BLEND);
    fr(0,0,WIN_W,WIN_H,0x000000b0);
    SDL_SetRenderDrawBlendMode(g_rend,SDL_BLENDMODE_NONE);

    /* Outside click: if license open, close license; else close about */
    if(saved_click && !pt_in(g_mouse.x,g_mouse.y,px,py,pw,ph)){
        if(c->license_popup_open){ c->license_popup_open=0; }
        else { c->about_popup_open=0; }
        g_mouse.clicked=0; return;
    }
    /* If license popup is open and click is inside about but outside license, close license */
    if(c->license_popup_open && saved_click){
        int lpw=600,lph=420,lpx2=(WIN_W-lpw)/2,lpy2=(WIN_H-lph)/2;
        if(!pt_in(g_mouse.x,g_mouse.y,lpx2,lpy2,lpw,lph)){
            c->license_popup_open=0; g_mouse.clicked=0;
        }
    }
    g_popup_blocked=0;
    g_mouse.clicked=saved_click;

    surface(px,py,pw,ph,0x111827ff,0x42516bff);
    dtc(px+pw/2,py+20,"LineClash",COL_WHITE,2);
    {char version[64];snprintf(version,sizeof(version),"v%s  /  CREDITS",TETRINETZ_VERSION);dtc(px+pw/2,py+54,version,0xbba5f4ff,1);}
    Uint32 now=SDL_GetTicks();
    if(c->credits_tick && !c->credits_paused && !c->license_popup_open){Uint32 elapsed=now-c->credits_tick;if(elapsed>100)elapsed=100;c->credits_offset+=elapsed*.026f;}
    c->credits_tick=now;
    SDL_Rect clip={px+20,py+86,pw-40,ph-158};SDL_RenderSetClipRect(g_rend,&clip);
    int cy=py+115-(int)c->credits_offset,center=px+pw/2;
    dtc(center,cy,"CREATED BY",0xbba5f4ff,1);cy+=30;
    dtc(center,cy,"Tommy Frössman",COL_WHITE,2);cy+=42;
    dtc(center,cy,"frossmant / styrbjorn",COL_GRAY,1);cy+=60;
    dtc(center,cy,"Stack blocks. Clear lines. Challenge your friends.",COL_LTGRAY,1);cy+=64;
    dtc(center,cy,"SOUNDTRACK",0xbba5f4ff,1);cy+=40;
    for(int i=0;i<music_track_count();i++){
        const MusicTrack *track=music_track(i);
        dtc(center,cy,track->title,COL_WHITE,1);cy+=23;
        dtc(center,cy,track->artist,0x9af0d2ff,1);cy+=21;
        dtc(center,cy,track->license,COL_GRAY,1);
        if(g_mouse.clicked && g_mouse.y>=clip.y && g_mouse.y<clip.y+clip.h && pt_in(g_mouse.x,g_mouse.y,px+20,cy-44,pw-40,64)){music_link(track->source_url);g_mouse.clicked=0;}
        cy+=48;
    }
    dtc(center,cy,"Alexandr Zhelanov",COL_WHITE,1);cy+=24;
    dtc(center,cy,"soundcloud.com/alexandr-zhelanov",COL_GRAY,1);cy+=58;
    dtc(center,cy,"TYPOGRAPHY",0xbba5f4ff,1);cy+=30;
    dtc(center,cy,"Roboto / SIL Open Font License 1.1",COL_WHITE,1);cy+=60;
    dtc(center,cy,"FREE & OPEN SOURCE",0xbba5f4ff,1);cy+=30;
    dtc(center,cy,"GNU General Public License v3 or later",COL_WHITE,1);cy+=26;
    dtc(center,cy,"Music and fonts retain their own licenses.",COL_GRAY,1);cy+=66;
    dtc(center,cy,"Thank you for playing.",COL_WHITE,2);cy+=60;
    SDL_RenderSetClipRect(g_rend,NULL);
    if(cy<clip.y)c->credits_offset=-(float)clip.h;
    fr(px+20,py+80,pw-40,1,0x303c54ff);
    fr(px+20,py+ph-64,pw-40,1,0x303c54ff);
    if(dbtn(px+20,py+ph-48,106,32,c->credits_paused?"Resume":"Pause",BS_NORMAL))c->credits_paused=!c->credits_paused;
    if(dbtn(px+134,py+ph-48,92,32,"Restart",BS_NORMAL))c->credits_offset=0;
    if(dbtn(px+234,py+ph-48,90,32,"License",BS_NORMAL))c->license_popup_open=1;
    if(dbtn(px+332,py+ph-48,90,32,"Source",BS_NORMAL))music_link("https://github.com/kladhest-se/lineclash");
    if(dbtn(px+pw-118,py+ph-48,98,32,"Close",BS_ACTIVE)){c->about_popup_open=0;c->license_popup_open=0;}
    g_mouse.clicked=0; /* consume remaining click */

    /* ── LICENSE sub-popup ── */
    if(c->license_popup_open){
        static const char *LICENSE_LINES[]={
            "GNU GENERAL PUBLIC LICENSE",
            "Version 3, 29 June 2007",
            "",
            "Copyright (C) 2024  frossmant",
            "https://github.com/kladhest-se/lineclash",
            "",
            "This program is free software: you can redistribute it",
            "and/or modify it under the terms of the GNU General",
            "Public License as published by the Free Software",
            "Foundation, either version 3 of the License, or",
            "(at your option) any later version.",
            "",
            "This program is distributed in the hope that it will",
            "be useful, but WITHOUT ANY WARRANTY; without even the",
            "implied warranty of MERCHANTABILITY or FITNESS FOR A",
            "PARTICULAR PURPOSE. See the GNU General Public License",
            "for more details.",
            "",
            "You should have received a copy of the GNU General",
            "Public License along with this program. If not, see:",
            "https://www.gnu.org/licenses/gpl-3.0.txt",
            "",
            "                   TERMS AND CONDITIONS",
            "",
            "0. Definitions.",
            "",
            "\"This License\" refers to version 3 of the GNU General",
            "Public License.",
            "",
            "\"The Program\" refers to any copyrightable work licensed",
            "under this License.",
            "",
            "1. Source Code.",
            "",
            "The \"source code\" for a work means the preferred form",
            "of the work for making modifications to it. \"Object",
            "code\" means any non-source form of a work.",
            "",
            "2. Basic Permissions.",
            "",
            "All rights granted under this License are granted for",
            "the term of copyright on the Program, and are",
            "irrevocable provided the stated conditions are met.",
            "This License explicitly affirms your unlimited",
            "permission to run the unmodified Program.",
            "",
            "Full license text: https://www.gnu.org/licenses/",
            NULL
        };
        int nlines=0; while(LICENSE_LINES[nlines]) nlines++;
        int lpw=600,lph=440,lpx=(WIN_W-lpw)/2,lpy=(WIN_H-lph)/2;
        const int VIS=21, LINE_H=16;
        const int CLOSE_H=32, CLOSE_MARGIN=26;
        const int CONTENT_H=lph-36-CLOSE_H-CLOSE_MARGIN*2; /* text area height */
        SDL_SetRenderDrawBlendMode(g_rend,SDL_BLENDMODE_BLEND);
        fr(0,0,WIN_W,WIN_H,0x000000a0);
        SDL_SetRenderDrawBlendMode(g_rend,SDL_BLENDMODE_NONE);
        /* Fill body, then header, then draw full border on top */
        fr(lpx,lpy,lpw,lph,0x0c0c1eff);
        fr(lpx,lpy,lpw,28,0x14241aff);
        dr(lpx,lpy,lpw,lph,0x70c070ff); /* border drawn last — complete rectangle */
        dtc(lpx+lpw/2,lpy+8,"GNU General Public License v3",0x70c070ff,1);
        /* scroll area */
        int max_scroll=nlines-VIS; if(max_scroll<0)max_scroll=0;
        if(c->license_scroll>max_scroll) c->license_scroll=max_scroll;
        if(c->license_scroll<0) c->license_scroll=0;
        for(int li=0;li<VIS&&(li+c->license_scroll)<nlines;li++){
            const char *line=LICENSE_LINES[li+c->license_scroll];
            uint32_t col=(*line=='h')?0x60c060ff:
                         (*line==' '||*line=='"')?0x9090c0ff:
                         (li==0&&c->license_scroll==0)?0xa0ffa0ff:0x8080a0ff;
            dt(lpx+14,lpy+34+li*LINE_H,line,col,1);
        }
        /* scroll bar */
        if(nlines>VIS){
            int sbh=CONTENT_H, sby=lpy+34;
            int th=(sbh*VIS)/nlines;
            int ty2=sby+(sbh*c->license_scroll)/nlines;
            fr(lpx+lpw-10,sby,6,sbh,0x1a1a30ff);
            fr(lpx+lpw-10,ty2,6,th,0x70c070ff);
            if(g_mouse.held&&pt_in(g_mouse.x,g_mouse.y,lpx+lpw-12,sby,10,sbh)){
                c->license_scroll=(int)((g_mouse.y-sby)*nlines/sbh);
                if(c->license_scroll>max_scroll) c->license_scroll=max_scroll;
            }
        }
        /* Close button with clear margin from text area */
        if(dbtn(lpx+lpw/2-60,lpy+lph-CLOSE_H-CLOSE_MARGIN,120,CLOSE_H,"Close",BS_ACTIVE))
            c->license_popup_open=0;
    }
}

void sdl_render_server_list(ClientCtx*c){
    BtnId action=BTN_NONE;
    const int CX    = WIN_W/2;
    const int LSX   = 32;
    const int LSW   = WIN_W-64;
    const int ROW_H = 62;
    const int HDR_Y = 236;
    const int LIST_Y= HDR_Y+32;

    /* If any popup is open, swallow background clicks entirely so
     * underlying buttons cannot fire while a popup is visible. */
    int any_popup = (c->server_popup_open || c->about_popup_open || c->keymap_popup_open || c->music_player_open);
    int saved_click = g_mouse.clicked;
    g_popup_blocked = any_popup; /* suppress hover+click on background */
    if (any_popup) g_mouse.clicked = 0;
    /* Per-row button strip (right side): Refresh + Rename + Remove */
    const int BTN_W_REF = 66;   /* wider for "Refresh" text */
    const int BTN_W     = 58;   /* Rename / Remove */
    const int BTN_H     = 26;
    const int BUTTONS_W = BTN_W_REF + BTN_W*2 + 16;
    /* Content columns */
    const int COL_NAME = LSX+18;
    const int COL_STAT = COL_NAME+260;
    const int COL_REG  = COL_STAT+150;  /* registered users */
    const int COL_PLAY = COL_REG+110;   /* playing — wider gap */
    const int COL_IDLE = COL_PLAY+90;  /* idle — wider gap */

    SDL_SetRenderDrawColor(g_rend,0x0d,0x0d,0x1a,0xff);
    SDL_RenderClear(g_rend);

    int loading=directory_busy() || ((c->srv_test_state>=1 && c->srv_test_state<=3)||c->srv_test_state==10);
    /* Brand and compact audio/settings controls. */
    fr(0,0,WIN_W,88,0x111b28ff);fr(32,87,WIN_W-64,1,0x25354aff);
    brick_logo(32,26,13,3,loading);
    dt(75,22,"LineClash",0xf0f5fcff,2);
    dt(76,56,"MULTIPLAYER FALLING BLOCKS",0x91a4bfff,1);
    dt(32,109,"Your next match starts here.",0xf0f5fcff,2);
    if(dbtn(32,146,166,30,"Public servers",c->server_tab==0?BS_ACTIVE:BS_NORMAL))action=BTN_PUBLIC_SERVERS;
    if(dbtn(208,146,166,30,"Saved servers",c->server_tab==1?BS_ACTIVE:BS_NORMAL))action=BTN_SAVED_SERVERS;
    if(dbtn(384,146,166,30,"Local network",c->server_tab==2?BS_ACTIVE:BS_NORMAL))action=BTN_LAN_SERVERS;
    if(dbtn(WIN_W-96,22,64,28,"Quit",BS_NORMAL))        action=BTN_QUIT;
    if(dbtn(WIN_W-166,22,62,28,"About",BS_NORMAL))      action=BTN_SRV_ABOUT;
    if(dbtn(WIN_W-498,22,104,28,c->central_profile[0]?c->central_profile:"Accounts",BS_NORMAL))client_accounts_open(c,0);
    if(dbtn(WIN_W-238,22,64,28,"Settings",BS_NORMAL))     action=BTN_KEYMAP_POPUP;
    {BtnStyle ss=c->sound_enabled?BS_TOGON:BS_NORMAL;
     if(dbtn(WIN_W-310,22,64,28,"SFX",ss))              action=BTN_TOGGLE_SOUND;}
    {BtnStyle ms=music_get_enabled()?BS_TOGON:BS_NORMAL;
     if(dbtn(WIN_W-382,22,64,28,"Music",ms))            action=BTN_TOGGLE_MUSIC;}



    /* SFX + Music volume sliders — beneath the toggle buttons, inside the title bar */
    {int sx=WIN_W-310,sy=57,sw=64,sh=7;
     float _v=sound_get_sfx_volume();
     int track=sw-2; int filled=(int)(_v*track);
     fr(sx,sy,sw,sh,0x16182aff);        /* background */
     if(filled>0) fr(sx+1,sy+1,filled,sh-2,0x3a5a9aff); /* filled */
     fr(sx+1+filled,sy+1,track-filled,sh-2,0x1c1e30ff); /* empty  */
     dr(sx,sy,sw,sh,0x4a4a80ff);        /* border on top */
     if(g_mouse.held&&pt_in(g_mouse.x,g_mouse.y,sx,sy,sw,sh)) c->sfx_slider_drag=1;
     if(!g_mouse.held) c->sfx_slider_drag=0;
     if(c->sfx_slider_drag){
         float nv=(float)(g_mouse.x-(sx+1))/track;
         if(nv<0.0f)nv=0.0f; else if(nv>1.0f)nv=1.0f;
         sound_set_sfx_volume(nv); client_save_settings(c);}
    }
    {int sx=WIN_W-382,sy=57,sw=64,sh=7;
     float _v=music_get_volume();
     int track=sw-2; int filled=(int)(_v*track);
     fr(sx,sy,sw,sh,0x16182aff);        /* background */
     if(filled>0) fr(sx+1,sy+1,filled,sh-2,0x3a5a9aff); /* filled */
     fr(sx+1+filled,sy+1,track-filled,sh-2,0x1c1e30ff); /* empty  */
     dr(sx,sy,sw,sh,0x4a4a80ff);        /* border on top */
     if(g_mouse.held&&pt_in(g_mouse.x,g_mouse.y,sx,sy,sw,sh)) c->music_slider_drag=1;
     if(!g_mouse.held) c->music_slider_drag=0;
     if(c->music_slider_drag){
         float nv=(float)(g_mouse.x-(sx+1))/track;
         if(nv<0.0f)nv=0.0f; else if(nv>1.0f)nv=1.0f;
         music_set_volume(nv); client_save_settings(c);}
    }

    if(dbtn(LSX,188,144,32,c->server_tab==1?"+ Add server":"Refresh list",BS_ACTIVE)) action=c->server_tab==1?BTN_SRV_ADD:c->server_tab==2?BTN_LAN_SERVERS:BTN_DISCOVER;
    {const char *labels[]={"All servers","Favourites","Recent","Open slots","Ping <=100ms"};
     if(dbtn(LSX+154,188,148,32,labels[c->server_filter%5],BS_NORMAL)) action=BTN_SERVER_FILTER;}
    {const char *labels[]={"Default order","Low latency","Last played"};
     if(dbtn(LSX+312,188,148,32,labels[c->server_sort%3],BS_NORMAL)) action=BTN_SERVER_SORT;}

    const char *directory_url=SDL_getenv("LINECLASH_DIRECTORY_URL");
    if(directory_url && directory_url_valid(directory_url,0) && dbtn(WIN_W-498,22,104,28,"Browse web",BS_NORMAL)) music_link(directory_url);

    {char count[48];int total=0;for(int n=0;n<c->server_count;n++)if(c->server_tab==2?c->server_list[n].lan:c->server_tab==1?(c->server_list[n].locally_saved||(!c->server_list[n].discovered&&!c->server_list[n].lan)):c->server_list[n].discovered)total++;snprintf(count,sizeof(count),"%d %s SERVERS",total,c->server_tab==2?"LOCAL":c->server_tab==1?"SAVED":"PUBLIC");
     dt(WIN_W-190,137,count,0xbba5f4ff,1);}
    /* Quiet table header lets server names carry the visual weight. */
    fr(LSX,HDR_Y,LSW,32,0x171f30ff);
    dt(COL_NAME,HDR_Y+8,"Game server",   0x5050a0ff,1);
    dt(COL_STAT,HDR_Y+8,"Status",         0x5050a0ff,1);
    dt(COL_REG, HDR_Y+8,"Registered",     0x5050a0ff,1);
    dt(COL_PLAY,HDR_Y+8,"Playing",        0x5050a0ff,1);
    dt(COL_IDLE,HDR_Y+8,"Idle",           0x5050a0ff,1);

    /* Pagination */
    int visible[MAX_SAVED_SERVERS],visible_count=client_server_order(c,visible);
    int selected_visible=0;
    for(int n=0;n<visible_count;n++)if(visible[n]==c->server_cursor)selected_visible=1;
    if(!selected_visible)c->server_cursor=visible_count?visible[0]:-1;
    const int SRVS_PER_PAGE = 6;
    {int tp=(visible_count>0?(visible_count+SRVS_PER_PAGE-1)/SRVS_PER_PAGE:1);
     if(c->srv_page>=tp) c->srv_page=tp-1;
     if(c->srv_page<0)  c->srv_page=0;}
    int srv_start = c->srv_page * SRVS_PER_PAGE;
    int srv_end   = srv_start + SRVS_PER_PAGE;
    if(srv_end > visible_count) srv_end = visible_count;
    int srv_total_pages = visible_count>0?(visible_count+SRVS_PER_PAGE-1)/SRVS_PER_PAGE:1;

    /* Pagination arrows + indicator */
    if(srv_total_pages > 1){
        int ay = LIST_Y + (SRVS_PER_PAGE * ROW_H)/2 - 9;
        {BtnStyle bs=c->srv_page>0?BS_NORMAL:BS_DISABLED;
         if(dbtn(LSX-28,ay,22,18,"<",bs)){
             if(c->srv_page>0){c->srv_page--;c->server_cursor=visible[c->srv_page*SRVS_PER_PAGE];}}}
        {BtnStyle bs=c->srv_page<srv_total_pages-1?BS_NORMAL:BS_DISABLED;
         if(dbtn(LSX+LSW+6,ay,22,18,">",bs)){
             if(c->srv_page<srv_total_pages-1){c->srv_page++;c->server_cursor=visible[c->srv_page*SRVS_PER_PAGE];}}}
    }

    /* Server rows */
    int i;
    for(int vi=srv_start;vi<srv_end;vi++){
        i=visible[vi];
        SavedServer *ss=&c->server_list[i];
        int row_idx = vi - srv_start;
        int ry     = LIST_Y + row_idx*ROW_H;
        int is_cur = (i==c->server_cursor);

        /* Determine if connection is allowed — only block if explicitly failed */
        int blocked = 0; /* Retry is available even after a failed probe. */

        uint32_t bg = is_cur ? 0x2b2b47ff : 0x182133ff;
        uint32_t bo = is_cur ? (blocked?0xa03030ff:0x9581c7ff) : 0x2c3950ff;
        surface(LSX,ry+3,LSW,ROW_H-6,bg,bo);
        if(is_cur)round_fill(LSX+1,ry+13,3,ROW_H-26,1,0xbba5f4ff);

        /* Click left area = select; selected + click = connect (if allowed) */
        int cw = LSW - BUTTONS_W - 6;
        if(pt_in(g_mouse.x,g_mouse.y,LSX,ry,cw,ROW_H-1)&&g_mouse.clicked){
            if(is_cur && !blocked) action=BTN_SRV_CONNECT;
            else { c->server_cursor=i; c->srv_page=vi/SRVS_PER_PAGE; }
        }

        /* Name (top) + host:port (sub-line) */

        uint32_t name_col = blocked ? 0x606060ff : (is_cur?COL_WHITE:COL_LTGRAY);
        {char label[31];snprintf(label,sizeof(label),"%.30s",ss->name[0]?ss->name:ss->host);dt(COL_NAME,ry+11,label,name_col,1);}
        {char sub[100]; snprintf(sub,sizeof(sub),"%s%.21s:%d",ss->favourite?"* ":"",ss->host,ss->port);
         dt(COL_NAME, ry+11+FH+2, sub, 0x383858ff, 1);}

        /* Status dot + label */
        {int dy=ry+(ROW_H-1)/2-5;
         const char *slabel; uint32_t dot_in,dot_out,txt_col;
         if(ss->online==1&&ss->auth_status>=0){
             slabel="Online"; dot_in=0x003300ff; dot_out=0x00cc44ff; txt_col=COL_OPEN_GREEN;
         } else if(ss->auth_status==-1){
             slabel="Auth failed"; dot_in=0x330000ff; dot_out=0xcc2200ff; txt_col=COL_INGAME_RED;
         } else if(ss->auth_status==-2){
             slabel="Ver mismatch"; dot_in=0x332200ff; dot_out=0xcc8800ff; txt_col=0xffaa00ff;
         } else if(ss->auth_status==-3||ss->online==-1){
             slabel="Unreachable"; dot_in=0x330000ff; dot_out=0x880000ff; txt_col=0x886666ff;
         } else {
             slabel="--"; dot_in=0x1a1a00ff; dot_out=0x555522ff; txt_col=0x444433ff;
         }
         slabel=client_server_status(ss);
         fr(COL_STAT,dy,10,10,dot_in); dr(COL_STAT,dy,10,10,dot_out);
         dt(COL_STAT+14,dy,slabel,txt_col,1);
         char detail[40];if(ss->ping_ms>0)snprintf(detail,sizeof(detail),"%dms  %d/%d",ss->ping_ms,ss->users_playing+ss->users_idle,ss->capacity);else snprintf(detail,sizeof(detail),"Latency unknown");
         dt(COL_STAT,ry+37,detail,COL_GRAY,1);}

        /* User counts (only when online) */
        if(ss->online==1&&ss->auth_status>=0){
            int dy=ry+(ROW_H-1)/2-FH/2;
            {char b[8];snprintf(b,sizeof(b),"%d",ss->users_registered);dt(COL_REG,dy,b,0x6060a0ff,1);}
            {char b[8];snprintf(b,sizeof(b),"%d",ss->users_playing);   dt(COL_PLAY,dy,b,COL_OPEN_GREEN,1);}
            {char b[8];snprintf(b,sizeof(b),"%d",ss->users_idle);      dt(COL_IDLE,dy,b,0x5050a0ff,1);}
        } else {
            int dy=ry+(ROW_H-1)/2-FH/2;
            dt(COL_REG,dy,"--",0x2a2a44ff,1);
        }

        /* Per-row [Refresh] [Rename] [Remove] buttons */
        if(c->server_tab==1) {int bx_rem = LSX+LSW-BTN_W-4;
         int bx_ren = bx_rem-BTN_W-6;
         int bx_ref = bx_ren-BTN_W_REF-6;
         int by     = ry+(ROW_H-1-BTN_H)/2;
         if(dbtn(bx_ref,by,BTN_W_REF,BTN_H,"Refresh",BS_NORMAL))
             {c->server_cursor=i; action=BTN_SRV_REFRESH;}
         if(dbtn(bx_ren,by,BTN_W,BTN_H,"Edit",BS_NORMAL))
             {c->server_cursor=i; action=BTN_SRV_EDIT;}
         if(dbtn(bx_rem,by,BTN_W,BTN_H,"Remove",BS_NORMAL))
             {c->server_cursor=i; action=BTN_SRV_DEL;}
        }
    }

    if(visible_count==0) {
        surface(LSX,LIST_Y+6,LSW,ROW_H*6-10,0x111c2aff,0x25354aff);
        int icon_y=LIST_Y+65;
        brick_logo(CX-33,icon_y,30,6,loading);
        dtc(CX,LIST_Y+160,loading?"Finding your next match…":c->server_tab==2?"Find a game nearby.":c->server_tab==1?"Your private server collection.":"Find a community to play with.",0xe9f1fcff,2);
        dtc(CX,LIST_Y+207,c->server_tab==2?"Search your local network for a LineClash server.":c->server_tab==1?"Add a host and port to keep your own server list.":"Refresh the public list, or try another filter.",0x97abc5ff,1);
        if(c->server_tab==1 && dbtn(CX-176,LIST_Y+250,168,38,"+ Add a server",BS_ACTIVE))action=BTN_SRV_ADD;
        if(c->server_tab!=1 && dbtn(CX-84,LIST_Y+250,168,38,"Refresh list",BS_ACTIVE))action=c->server_tab==2?BTN_LAN_SERVERS:BTN_DISCOVER;
    }
    /* Bottom actions are visible when there is a server to select. */
    if(visible_count>0) {int bot_y = LIST_Y + SRVS_PER_PAGE*ROW_H + 12;
     if(bot_y>WIN_H-56) bot_y=WIN_H-56;
     int half=(LSW-8)/3;
     if(dbtn(LSX,bot_y,half,38,c->server_tab?"+ Add a server":"Saved servers",BS_NORMAL)) action=c->server_tab?BTN_SRV_ADD:BTN_SAVED_SERVERS;
     if(dbtn(LSX+half+4,bot_y,half,38,"Toggle favourite",BS_NORMAL)) action=BTN_FAVOURITE;
     int can_join=(c->server_cursor>=0&&c->server_cursor<c->server_count&&
                   c->server_list[c->server_cursor].online>=0&&
                   c->server_list[c->server_cursor].auth_status>=0);
     if(dbtn(LSX+half*2+8,bot_y,LSW-half*2-8,38,"Join server",
             can_join?BS_ACTIVE:BS_DISABLED)&&can_join) action=BTN_SRV_CONNECT;
    }

    dt(LSX,LIST_Y+SRVS_PER_PAGE*ROW_H+62,c->directory_message,0x97abc5ff,1);
    fr(32,754,WIN_W-64,1,0x25354aff);
    {char footer[120];snprintf(footer,sizeof(footer),"LINECLASH  /  v%s  /  COMMUNITY MULTIPLAYER",TETRINETZ_VERSION);dt(32,768,footer,0x8096b2ff,1);}
    dt(WIN_W-260,768,"Build connections. Break lines.",0x8096b2ff,1);

    /* ================================================================
     * ADD popup — 4 fields: host, port, nick, password
     * No alias; server's own name is used automatically.
     * ================================================================ */
    if(c->server_popup_open==1){
        int pw=480,ph=316,px=(WIN_W-pw)/2,py=(WIN_H-ph)/2;
        SDL_SetRenderDrawBlendMode(g_rend,SDL_BLENDMODE_BLEND);
        fr(0,0,WIN_W,WIN_H,0x000000b0);
        SDL_SetRenderDrawBlendMode(g_rend,SDL_BLENDMODE_NONE);
        /* Restore click for popup elements only; outside click closes popup */
        g_mouse.clicked = saved_click;
        if(saved_click && !pt_in(g_mouse.x,g_mouse.y,px,py,pw,ph)){
            c->server_popup_open=0; g_mouse.clicked=0; goto done_srv_list; }
        g_popup_blocked=0; /* popup content is interactive */
        fr(px,py,pw,ph,0x10102aff);
        fr(px,py,pw,26,0x1a1a40ff);  /* header bar */
        dr(px,py,pw,ph,COL_PIECE_I); /* border after header — covers all edges */
        dtc(px+pw/2,py+7,c->server_edit_idx>=0?"Edit saved server":"Add saved server",COL_LTGRAY,1);

        const char *lbls[4]={"Host / IP address","Port","Username","Password"};
        const char *phs [4]={"192.168.1.10","31457","(optional)","(optional; blank keeps saved password)"};
        TextField  *tfs [4]={&c->tf_srv_host,&c->tf_srv_port,
                             &c->tf_srv_nick,&c->tf_srv_pass};
        int is_pass[4]={0,0,0,1};
        int fi, FR=52;
        /* Disable fields while testing */
        int testing=(c->srv_test_state==1||c->srv_test_state==2);
        for(fi=0;fi<4;fi++){
            int fy=py+32+fi*FR;
            int foc=(c->tf_srv_focus==fi)&&!testing;
            int fw2=pw-28;
            dt(px+14,fy,lbls[fi],foc?0x7070c0ff:0x4040a0ff,1);
            uint32_t fbg=testing?0x080810ff:foc?0x1a1a38ff:0x0e0e1eff;
            uint32_t fbo=testing?0x1a1a28ff:foc?0x7070c0ff:0x222244ff;
            fr(px+14,fy+16,fw2,26,fbg); dr(px+14,fy+16,fw2,26,fbo);
            if(!testing&&pt_in(g_mouse.x,g_mouse.y,px+14,fy+16,fw2,26)&&g_mouse.clicked)
                c->tf_srv_focus=fi;
            {char disp[130]="";
             if(tfs[fi]->len>0){
                 if(is_pass[fi]){int k;for(k=0;k<tfs[fi]->len&&k<127;k++)disp[k]='*';
                     if(foc){disp[tfs[fi]->len]='_';disp[tfs[fi]->len+1]='\0';}else disp[tfs[fi]->len]='\0';}
                 else snprintf(disp,sizeof(disp),"%s%s",tfs[fi]->text,foc?"_":"");
                 dt(px+18,fy+21,disp,testing?0x505060ff:COL_WHITE,1);
             } else {
                 dt(px+18,fy+21,phs[fi],0x303050ff,1);
                 if(foc) dt(px+18,fy+21,"_",0x6060a0ff,1);}
            }
        }

        /* Status / error message area */
        {int my=py+ph-88;
         if(c->srv_test_state==1||c->srv_test_state==2){
             /* Spinner dots */
             int dots=(c->srv_test_ticks/10)%4;
             char spin[32]; snprintf(spin,sizeof(spin),"%s%.*s",c->srv_test_msg,dots,"...");
             dtc(px+pw/2,my+8,spin,0x8080c0ff,1);
         } else if(c->srv_test_state==-1){
             dtc(px+pw/2,my+8,c->srv_test_msg,COL_INGAME_RED,1);
         }}

        /* Buttons */
        {int bby=py+ph-50; int bw2=(pw-42)/2;
         BtnStyle add_bs=testing?BS_DISABLED:BS_ACTIVE;
         if(dbtn(px+14,    bby,bw2,34,"Save server",add_bs)&&!testing) action=BTN_SRV_ADD_OK;
         if(dbtn(px+28+bw2,bby,bw2,34,"Cancel",BS_NORMAL)){
             c->server_popup_open=0;
             if(c->srv_test_sock!=INVALID_SOCK){sock_close(c->srv_test_sock);c->srv_test_sock=INVALID_SOCK;}
             c->srv_test_state=0;
         }}
    }

    /* ================================================================
     * RENAME popup — single alias field
     * ================================================================ */
    if(c->server_popup_open==3){
        int pw=420,ph=140,px=(WIN_W-pw)/2,py=(WIN_H-ph)/2;
        SDL_SetRenderDrawBlendMode(g_rend,SDL_BLENDMODE_BLEND);
        fr(0,0,WIN_W,WIN_H,0x000000b0);
        SDL_SetRenderDrawBlendMode(g_rend,SDL_BLENDMODE_NONE);
        g_mouse.clicked = saved_click;
        if(saved_click && !pt_in(g_mouse.x,g_mouse.y,px,py,pw,ph)){
            c->server_popup_open=0; c->server_edit_idx=-1; g_mouse.clicked=0; goto done_srv_list; }
        g_popup_blocked=0;
        fr(px,py,pw,ph,0x10102aff);
        fr(px,py,pw,26,0x1a1a40ff);
        dr(px,py,pw,ph,COL_PIECE_I);
        {char title[80];
         const char *sname=(c->server_edit_idx>=0&&c->server_edit_idx<c->server_count)
             ?c->server_list[c->server_edit_idx].host:"server";
         snprintf(title,sizeof(title),"Rename — %s",sname);
         dtc(px+pw/2,py+7,title,COL_LTGRAY,1);}
        dt(px+14,py+34,"Alias (leave blank to use server name)",0x4040a0ff,1);
        {int foc=c->tf_srv_rename.focused;
         int fw2=pw-28;
         fr(px+14,py+50,fw2,28,foc?0x1a1a38ff:0x0e0e1eff);
         dr(px+14,py+50,fw2,28,foc?0x7070c0ff:0x222244ff);
         if(pt_in(g_mouse.x,g_mouse.y,px+14,py+50,fw2,28)&&g_mouse.clicked)
             c->tf_srv_rename.focused=1;
         {char disp[130];
          snprintf(disp,sizeof(disp),"%s%s",c->tf_srv_rename.text,foc?"_":"");
          if(c->tf_srv_rename.len>0) dt(px+18,py+56,disp,COL_WHITE,1);
          else { dt(px+18,py+56,"(server name)",0x303050ff,1);
                 if(foc) dt(px+18,py+56,"_",0x6060a0ff,1);}}
        }
        {int bby=py+ph-44; int bw2=(pw-42)/2;
         if(dbtn(px+14,    bby,bw2,32,"Rename",BS_ACTIVE)) action=BTN_SRV_RENAME_OK;
         if(dbtn(px+28+bw2,bby,bw2,32,"Cancel",BS_NORMAL)){c->server_popup_open=0;c->server_edit_idx=-1;}
        }
    }

    /* ── ABOUT POPUP (server list) ── */
    if(c->about_popup_open) { render_about_popup(c, saved_click); }

    if((c->state==CS_PLAYING || c->state==CS_GAMEOVER) && c->music_player_open) render_music_popup(c,saved_click);

    /* ── KEYMAP POPUP (server list) ── */
    if(c->keymap_popup_open) { render_keymap_popup(c, saved_click); }

    done_srv_list:;
    if(action!=BTN_NONE) client_on_button(c,action);
    SDL_RenderPresent(g_rend);
}
void sdl_render_connect(ClientCtx*c){
    BtnId action = BTN_NONE;
    int is_authing = (c->state == CS_AUTH);

    const int cx = WIN_W/2;
    const int fw = 320;   /* form width */
    const int fh = 28;    /* field height */
    const int lh = 14;    /* label height */
    const int lx = cx - fw/2;

    const int Y_SERVER_LBL = 140;
    const int Y_SERVER_FLD = Y_SERVER_LBL + lh;
    const int Y_PORT_LBL   = Y_SERVER_FLD + fh + 8;
    const int Y_PORT_FLD   = Y_PORT_LBL   + lh;
    const int Y_NICK_LBL   = Y_PORT_FLD   + fh + 8;
    const int Y_NICK_FLD   = Y_NICK_LBL   + lh;
    const int Y_PASS_LBL   = Y_NICK_FLD   + fh + 8;
    const int Y_PASS_FLD   = Y_PASS_LBL   + lh;
    const int Y_SOUND      = Y_PASS_FLD   + fh + 14;
    const int Y_SLIDER     = Y_SOUND      + 30 + 6;
    const int Y_CONNECT    = Y_SLIDER     + 30 + 8;
    const int Y_QUIT       = Y_CONNECT    + 38 + 4;
    const int Y_ERROR      = Y_QUIT       + 32 + 10;

    /* Panel height — tall enough to contain the error message too */
    const int PX = lx - 16;
    const int PY = Y_SERVER_LBL - 14;
    const int PW = fw + 32;
    const int PH = (Y_ERROR + 22 + 12) - PY;

    SDL_SetRenderDrawColor(g_rend, 0x0d, 0x0d, 0x1a, 0xff);
    SDL_RenderClear(g_rend);

    /* Title */
    fr(0,0,WIN_W,108,0x161d2dff); fr(32,107,WIN_W-64,1,0x39445eff);
    dts(cx - tw("LineClash",3)/2, 20, "LineClash", COL_PIECE_I, 3);
    {char ver[32]; snprintf(ver,sizeof(ver),"v%s",TETRINETZ_VERSION);
     dtc(cx, 76, ver, COL_GRAY, 1);}

    /* Form panel */
    surface(PX-12,PY-4,PW+24,PH+12,0x181f31ff,0x39445eff);

    if(is_authing && c->central_mode){
        dtc(cx,PY+48,"Signing in",COL_WHITE,2);
        dtc(cx,PY+120,c->central_profile,COL_HOSTGOLD,2);
        dtc(cx,PY+180,"Authorizing this server over HTTPS...",COL_LTGRAY,1);
        dtc(cx,PY+218,"Your password stays with the central service.",COL_GRAY,1);
        if(dbtn(lx,PY+290,fw,34,"Back to servers",BS_NORMAL))action=BTN_BACK_CONNECT;
    } else if (is_authing) {
        dtc(cx, PY+PH/2-10, "Authenticating...", COL_HOSTGOLD, 1);
        dtc(cx, PY+PH/2+10, "Please wait", COL_GRAY, 1);
    } else {
        /* Server — full width */
        dt(lx,Y_SERVER_LBL,"Server",COL_LTGRAY,1);
        if(draw_textfield(lx,Y_SERVER_FLD,fw,fh,&c->tf_host,"localhost")){
            c->tf_host.focused=1;c->tf_port.focused=c->tf_nick.focused=c->tf_pass.focused=0;}

        /* Port — full width (same as server) */
        dt(lx,Y_PORT_LBL,"Port",COL_LTGRAY,1);
        if(draw_textfield(lx,Y_PORT_FLD,fw,fh,&c->tf_port,"31457")){
            c->tf_port.focused=1;c->tf_host.focused=c->tf_nick.focused=c->tf_pass.focused=0;}

        /* Username */
        dt(lx,Y_NICK_LBL,"Username",COL_LTGRAY,1);
        if(draw_textfield(lx,Y_NICK_FLD,fw,fh,&c->tf_nick,"Player")){
            c->tf_nick.focused=1;c->tf_host.focused=c->tf_port.focused=c->tf_pass.focused=0;}

        /* Password */
        dt(lx,Y_PASS_LBL,"Password",COL_LTGRAY,1);
        {
            /* When credentials are cached for this username, show filled asterisks
               in the placeholder so it looks like the password is already entered */
            int creds_active = c->have_saved_creds
                             && strcmp(c->saved_nick, c->tf_nick.text)==0
                             && !c->tf_pass.len;
            if(draw_passfield(lx,Y_PASS_FLD,fw,fh,&c->tf_pass,
                              creds_active ? "********" : "")){
                c->tf_pass.focused=1;
                c->tf_host.focused=c->tf_port.focused=c->tf_nick.focused=0;
            }
        }

        /* SFX + Music — side by side */
        {int hw=(fw-4)/2;
         if(dtog(lx,Y_SOUND,hw,30,"SFX: OFF","SFX: ON",c->sound_enabled))
             action=BTN_TOGGLE_SOUND;
         {BtnStyle ms=music_get_enabled()?BS_TOGON:BS_NORMAL;
          if(dbtn(lx+hw+4,Y_SOUND,hw,30,"Music",ms)) action=BTN_TOGGLE_MUSIC;}}

        /* SFX + Music volume sliders — same width/height as buttons above */
        {int hw=(fw-4)/2;
         {int sx=lx,sy=Y_SLIDER,sw=hw,sh=30;
               float _v=sound_get_sfx_volume();
               int track=sw-2; int filled=(int)(_v*track);
               fr(sx,sy,sw,sh,0x16182aff);        /* background */
               if(filled>0) fr(sx+1,sy+1,filled,sh-2,0x3a5a9aff); /* filled */
               fr(sx+1+filled,sy+1,track-filled,sh-2,0x1c1e30ff); /* empty  */
               dr(sx,sy,sw,sh,0x4a4a80ff);        /* border on top */
               if(g_mouse.held&&pt_in(g_mouse.x,g_mouse.y,sx,sy,sw,sh)) c->sfx_slider_drag=1;
               if(!g_mouse.held) c->sfx_slider_drag=0;
               if(c->sfx_slider_drag){
                   float nv=(float)(g_mouse.x-(sx+1))/track;
                   if(nv<0.0f)nv=0.0f; else if(nv>1.0f)nv=1.0f;
                   sound_set_sfx_volume(nv); client_save_settings(c);}
         }
         {int sx=lx+hw+4,sy=Y_SLIDER,sw=hw,sh=30;
               float _v=music_get_volume();
               int track=sw-2; int filled=(int)(_v*track);
               fr(sx,sy,sw,sh,0x16182aff);        /* background */
               if(filled>0) fr(sx+1,sy+1,filled,sh-2,0x3a5a9aff); /* filled */
               fr(sx+1+filled,sy+1,track-filled,sh-2,0x1c1e30ff); /* empty  */
               dr(sx,sy,sw,sh,0x4a4a80ff);        /* border on top */
               if(g_mouse.held&&pt_in(g_mouse.x,g_mouse.y,sx,sy,sw,sh)) c->music_slider_drag=1;
               if(!g_mouse.held) c->music_slider_drag=0;
               if(c->music_slider_drag){
                   float nv=(float)(g_mouse.x-(sx+1))/track;
                   if(nv<0.0f)nv=0.0f; else if(nv>1.0f)nv=1.0f;
                   music_set_volume(nv); client_save_settings(c);}
         }}

        /* Connect — full width */
        if(dbtn(lx,Y_CONNECT,fw,36,"CONNECT",BS_ACTIVE)) action=BTN_CONNECT;

        /* Quit — full width */
        if(dbtn(lx,Y_QUIT,fw,28,"Use central account",BS_NORMAL)) action=BTN_CENTRAL_CONNECT;

        /* Error/success — word-wrapped to fit inside panel */
        if(c->connect_error[0]) {
            int is_ok = (strncmp(c->connect_error,"Account",7)==0 ||
                         strncmp(c->connect_error,"Logged",6)==0);
            uint32_t ecol = is_ok ? COL_OPEN_GREEN : COL_INGAME_RED;
            /* Truncate to panel width — ~fw/8 chars per line at scale 1 */
            char line1[80], line2[80];
            int clen = (int)strlen(c->connect_error);
            int chars_per_line = fw / (FW+1);
            if(clen <= chars_per_line){
                dtc(cx, Y_ERROR, c->connect_error, ecol, 1);
            } else {
                /* split at last space before chars_per_line */
                int split = chars_per_line;
                while(split>0 && c->connect_error[split]!=' ') split--;
                if(split==0) split=chars_per_line;
                strncpy(line1, c->connect_error, split); line1[split]='\0';
                strncpy(line2, c->connect_error+split+1, sizeof(line2)-1);
                dtc(cx, Y_ERROR,    line1, ecol, 1);
                dtc(cx, Y_ERROR+14, line2, ecol, 1);
            }
        }
    }

    dtc(cx,WIN_H-24,"Tab = next field     Enter = connect",COL_DKGRAY,1);
    if(action!=BTN_NONE) client_on_button(c,action);
    SDL_RenderPresent(g_rend);
}

/* ============================== LOBBY SELECT ============================== */
void sdl_render_lobby_select(ClientCtx*c){
    int i;
    BtnId action=BTN_NONE;

    /* Layout */
    const int TITLE_H = 80;
    const int list_x  = WIN_W/2 - 480;
    const int list_w  = 960;
    const int list_y  = TITLE_H + 16;
    const int hdr_h   = 22;
    const int row_h   = 64;

    const int ROOMS_PER_PAGE = 3;
    int total_pages = c->lobby_list_count > 0 ? (c->lobby_list_count+ROOMS_PER_PAGE-1)/ROOMS_PER_PAGE : 1;
    /* clamp page */
    if(c->lobby_page >= total_pages) c->lobby_page = total_pages-1;
    if(c->lobby_page < 0) c->lobby_page = 0;
    int page_start = c->lobby_page * ROOMS_PER_PAGE;
    int page_end   = page_start + ROOMS_PER_PAGE;
    if(page_end > c->lobby_list_count) page_end = c->lobby_list_count;
    int list_rows  = ROOMS_PER_PAGE; /* always 3 rows height for stable layout */
    int list_bot   = list_y + hdr_h + list_rows * row_h;
    int join_y     = list_bot + 8;
    int lb_y       = join_y + 44;   /* leaderboard below join button */

    SDL_SetRenderDrawColor(g_rend,0x0d,0x0d,0x1a,0xff);
    SDL_RenderClear(g_rend);

    /* Block all background interaction when any popup is open */
    int any_popup_early = (c->motd_popup_open||c->keymap_popup_open||c->about_popup_open||c->music_player_open);
    if(any_popup_early) g_popup_blocked=1;

    /* Title bar */
    fr(0,0,WIN_W,TITLE_H,0x12122aff); dr(0,0,WIN_W,TITLE_H,COL_BORDER);
    {int tx=20;
     dts(tx,12,"LineClash",COL_PIECE_I,3);
     {char sub[96]; snprintf(sub,sizeof(sub),"v%s  (%s)",TETRINETZ_VERSION,BUILD_DATE);
      int tw3=tw("LineClash",3);
      dtc(tx+tw3/2,58,sub,COL_GRAY,1);}}

    /* Top-right buttons: Keymap | Music | SFX | Quit | < Back */
    if(dbtn(WIN_W-74,4,66,22,"< Back",BS_NORMAL))  action=BTN_BACK_CONNECT;
    if(dbtn(WIN_W-146,4,66,22,"Quit",BS_NORMAL))   action=BTN_QUIT;
    if(dbtn(WIN_W-218,4,64,22,"About",BS_NORMAL))  action=BTN_SRV_ABOUT;
    if(dbtn(WIN_W-288,4,64,22,"Settings",BS_NORMAL)) action=BTN_KEYMAP_POPUP;
    {BtnStyle ss=c->sound_enabled?BS_TOGON:BS_NORMAL;
     if(dbtn(WIN_W-358,4,64,22,"SFX",ss))          action=BTN_TOGGLE_SOUND;}
    {BtnStyle ms=music_get_enabled()?BS_TOGON:BS_NORMAL;
     if(dbtn(WIN_W-428,4,64,22,"Music",ms))        action=BTN_TOGGLE_MUSIC;}
    /* SFX + Music volume sliders — beneath SFX/Music buttons */
    {int sx=WIN_W-358,sy=28,sw=64,sh=20;
     float _v=sound_get_sfx_volume();
     int track=sw-2; int filled=(int)(_v*track);
     fr(sx,sy,sw,sh,0x16182aff);        /* background */
     if(filled>0) fr(sx+1,sy+1,filled,sh-2,0x3a5a9aff); /* filled */
     fr(sx+1+filled,sy+1,track-filled,sh-2,0x1c1e30ff); /* empty  */
     dr(sx,sy,sw,sh,0x4a4a80ff);        /* border on top */
     if(g_mouse.held&&pt_in(g_mouse.x,g_mouse.y,sx,sy,sw,sh)) c->sfx_slider_drag=1;
     if(!g_mouse.held) c->sfx_slider_drag=0;
     if(c->sfx_slider_drag){
         float nv=(float)(g_mouse.x-(sx+1))/track;
         if(nv<0.0f)nv=0.0f; else if(nv>1.0f)nv=1.0f;
         sound_set_sfx_volume(nv); client_save_settings(c);}
    }
    {int sx=WIN_W-428,sy=28,sw=64,sh=20;
     float _v=music_get_volume();
     int track=sw-2; int filled=(int)(_v*track);
     fr(sx,sy,sw,sh,0x16182aff);        /* background */
     if(filled>0) fr(sx+1,sy+1,filled,sh-2,0x3a5a9aff); /* filled */
     fr(sx+1+filled,sy+1,track-filled,sh-2,0x1c1e30ff); /* empty  */
     dr(sx,sy,sw,sh,0x4a4a80ff);        /* border on top */
     if(g_mouse.held&&pt_in(g_mouse.x,g_mouse.y,sx,sy,sw,sh)) c->music_slider_drag=1;
     if(!g_mouse.held) c->music_slider_drag=0;
     if(c->music_slider_drag){
         float nv=(float)(g_mouse.x-(sx+1))/track;
         if(nv<0.0f)nv=0.0f; else if(nv>1.0f)nv=1.0f;
         music_set_volume(nv); client_save_settings(c);}
    }

    /* Logged-in-as strip — between title bar and room list */
    {int sy=TITLE_H+2;
     const char *plbl = c->priv_level==1 ? "[ADM]" :
                        c->priv_level==2 ? "[MGR]" : "[USR]";
     char uinfo[64];
     snprintf(uinfo,sizeof(uinfo),"Logged in as: %s  %s",c->auth_nick,plbl);
     dtc(WIN_W/2,sy+1,uinfo,COL_HOSTGOLD,1); /* all privilege badges gold */
    }

    /* Column headers */
    fr(list_x, list_y, list_w, hdr_h, 0x1a1a30ff);
    dr(list_x, list_y, list_w, hdr_h, COL_BORDER);
    dt(list_x+30,  list_y+5, "Room",    COL_LTGRAY, 1);
    dt(list_x+240, list_y+5, "Players", COL_LTGRAY, 1);
    dt(list_x+360, list_y+5, "Status",  COL_LTGRAY, 1);
    dt(list_x+460, list_y+5, "Mode",    COL_LTGRAY, 1);

    /* Lobby rows */
    if(!c->lobby_list_count)
        dtc(list_x+list_w/2, list_y+hdr_h+28, "Waiting for lobby list...", COL_GRAY, 1);

    /* Pagination arrows — only shown when more than ROOMS_PER_PAGE rooms */
    if(total_pages > 1){
        int ay = list_y + hdr_h + (ROOMS_PER_PAGE * row_h)/2 - 8;
        /* Left arrow */
        {BtnStyle bs = c->lobby_page > 0 ? BS_NORMAL : BS_DISABLED;
         if(dbtn(list_x - 28, ay, 22, 18, "<", bs))
             if(c->lobby_page>0){c->lobby_page--;c->lobby_cursor=c->lobby_page*ROOMS_PER_PAGE;}}
        /* Right arrow */
        {BtnStyle bs = c->lobby_page < total_pages-1 ? BS_NORMAL : BS_DISABLED;
         if(dbtn(list_x + list_w + 6, ay, 22, 18, ">", bs))
             if(c->lobby_page<total_pages-1){c->lobby_page++;c->lobby_cursor=c->lobby_page*ROOMS_PER_PAGE;}}
    }

    for(i=page_start; i<page_end; i++){
        LobbyInfo *lb = &c->lobby_list[i];
        int row_idx = i - page_start;
        int ry      = list_y + hdr_h + row_idx*row_h;
        int is_cur  = (i == c->lobby_cursor);
        int max_p   = lb->max_players > 0 ? lb->max_players : MAX_PLAYERS;
        int is_full = (lb->player_count >= max_p);
        BtnId rbid  = (BtnId)(BTN_LOBBY_ROW_0+i);

        /* Row bg + click */
        {int hov = pt_in(g_mouse.x,g_mouse.y, list_x,ry, list_w,row_h-2);
         uint32_t bg = is_cur?0x22224aff:hov?0x1a1a38ff:(i%2?0x12121cff:0x0f0f18ff);
         fr(list_x,ry,list_w,row_h-2,bg);
         dr(list_x,ry,list_w,row_h-2,is_cur?0x5050c0ff:COL_BORDER);
         if(hov&&g_mouse.clicked) action = is_cur ? BTN_LOBBY_JOIN : rbid;}

        if(is_cur) dt(list_x-12,ry+row_h/2-FH,">",COL_PIECE_I,2);

        /* ── Line 1: Room name  |  Status badge  |  Player count + bar ── */
        {char rname[MAX_LOBBY_NAME];
         snprintf(rname,sizeof(rname),"Room %d",lb->id);
         int mid=ry+row_h/2;
         dt(list_x+10,mid-FH*2-2,rname,is_cur?COL_WHITE:COL_LTGRAY,2);}

        /* Status badge — right of name, vertically centred */
        {const char *stat; uint32_t sc,sbg;
         if(lb->paused&&lb->in_game){stat="PAUSED";  sc=0xffaa00ff; sbg=0x2a1800ff;}
         else if(lb->in_game)       {stat="PLAYING"; sc=COL_INGAME_RED;  sbg=0x2a0808ff;}
         else if(is_full)           {stat="FULL";    sc=COL_FULL_ORANGE; sbg=0x2a1800ff;}
         else                       {stat="WAITING"; sc=COL_OPEN_GREEN;  sbg=0x082008ff;}
         int sw=(int)strlen(stat)*(FW+1)+12;
         int mid=ry+row_h/2;
         int bx=list_x+list_w-sw-90;
         fr(bx,mid-10,sw,20,sbg); dr(bx,mid-10,sw,20,sc);
         dt(bx+6,mid-6,stat,sc,1);}

        /* Player count + fill bar — centred vertically in row */
        {char pc[12]; int bfw=76;
         uint32_t bc=lb->in_game?COL_INGAME_RED:is_full?COL_FULL_ORANGE:COL_OPEN_GREEN;
         snprintf(pc,sizeof(pc),"%d/%d",lb->player_count,max_p);
         int px2=list_x+list_w-bfw-6;
         int mid=ry+row_h/2;               /* vertical centre of row */
         dt(px2,mid-FH*2-2,pc,bc,2);       /* text just above centre */
         {int bfill=lb->player_count*bfw/max_p;
          fr(px2,mid+5,bfw,4,0x202030ff);  /* bar 7px below centre */
          fr(px2,mid+5,bfill,4,bc);}}

        /* ── Line 2a (upper badge row): SPECIALS | LAST WINS | TAUNTS | SPEED UP ── */
        /* ── Line 2b (lower badge row): GARBAGE mode ── */
        #define CBADGE(lbl,on,sc_on,sc_off,bg_on,bg_off,bx2,by2)         { int _on=(on); uint32_t _sc=_on?(sc_on):(sc_off);           int _bw=tw((lbl),1)+10;           fr((bx2),(by2),_bw,15,_on?(bg_on):(bg_off)); dr((bx2),(by2),_bw,15,_sc);           dt((bx2)+5,(by2)+2,(lbl),_sc,1); (bx2)+=_bw+5; }

        {int bx2=list_x+10;
         int ya=ry+row_h-34; /* upper badge row */
         int yb=ry+row_h-17; /* lower badge row */

         /* Upper row: game option badges */
         {char lbl[24]; snprintf(lbl,sizeof(lbl),"SPECIALS %s",!lb->no_attacks?"ON":"OFF");
          CBADGE(lbl,!lb->no_attacks,COL_OPEN_GREEN,0x404060ff,0x061806ff,0x0a0a18ff,bx2,ya)}
         {char lbl[24]; snprintf(lbl,sizeof(lbl),"LAST WINS %s",lb->last_player_wins?"ON":"OFF");
          CBADGE(lbl,lb->last_player_wins,COL_OPEN_GREEN,0x404060ff,0x061806ff,0x0a0a18ff,bx2,ya)}
         {char lbl[20]; snprintf(lbl,sizeof(lbl),"TAUNTS %s",!lb->no_taunts?"ON":"OFF");
          CBADGE(lbl,!lb->no_taunts,0xffaa00ff,0x505040ff,0x1a0e00ff,0x0a0a18ff,bx2,ya)}
         {char lbl[20]; snprintf(lbl,sizeof(lbl),"SPEED UP %s",lb->speedup_enabled?"ON":"OFF");
          CBADGE(lbl,lb->speedup_enabled,0xffcc00ff,0x505040ff,0x1a1400ff,0x0a0a18ff,bx2,ya)}
         {char lbl[16]; snprintf(lbl,sizeof(lbl),"HOLD %s",lb->hold_enabled?"ON":"OFF");
          CBADGE(lbl,lb->hold_enabled,0x60c0ffff,0x404060ff,0x06181aff,0x0a0a18ff,bx2,ya)}

         /* Lower row: garbage mode + targeting */
         bx2=list_x+10;
         {int gdl=lb->garbage_system;
          char lbl[28]; snprintf(lbl,sizeof(lbl),"GARBAGE: %s",gdl?"GUIDELINE":"CLASSIC");
          uint32_t sc=gdl?0x8888ffff:0x6868a0ff;
          int bw=tw(lbl,1)+10;
          fr(bx2,yb,bw,15,gdl?0x08082aff:0x0c0c1aff);
          dr(bx2,yb,bw,15,sc);
          dt(bx2+5,yb+2,lbl,sc,1); bx2+=bw+5;}
         {const char *tgt_s=lb->garbage_targeting==0?"TARGET: ALL":lb->garbage_targeting==1?"TARGET: RANDOM":"TARGET: FOCUSED";
          uint32_t sc2=0x8060c0ff; int bw2=tw(tgt_s,1)+10;
          fr(bx2,yb,bw2,15,0x0c0820ff); dr(bx2,yb,bw2,15,sc2);
          dt(bx2+5,yb+2,tgt_s,sc2,1);}
        }
        #undef CBADGE
    }

    /* JOIN ROOM — full list width, directly below rows */
    if(c->lobby_list_count > 0){
        LobbyInfo *sel = &c->lobby_list[c->lobby_cursor];
        int max_p = sel->max_players>0 ? sel->max_players : MAX_PLAYERS;
        int can_join = !sel->in_game && sel->player_count < max_p;
        BtnStyle bs = can_join ? BS_ACTIVE : BS_DISABLED;
        if(dbtn(list_x, join_y, list_w, 36, "JOIN ROOM", bs) && can_join)
            action = BTN_LOBBY_JOIN;
    }

    /* Leaderboards + Players Online — three fixed-height panels side by side */
    {
        const int LB_H  = 200;
        const int gap   = 8;
        const int third = (list_w - gap*2) / 3;
        const int plx   = list_x;
        const int tlx   = list_x + third + gap;
        const int olx   = list_x + (third + gap)*2;
        const int pw    = third;
        const int tw2   = third;
        const int ow    = list_w - (third + gap)*2;
        const int row   = 16;
        const int hdr_rows = 2;
        int vis;

        /* ---- PLAYERS LEADERBOARD panel ---- */
        dpanel(plx, lb_y, pw, LB_H, "PLAYERS LEADERBOARD");
        {
            int hx  = plx+4;
            int hdy = lb_y+20;
            dt(hx+16, hdy, "Player", 0x5050a0ff, 1);
            dt(hx+pw-tw("Atk",1)-8-tw("W",1)-16, hdy, "W",   0x5050a0ff, 1);
            dt(hx+pw-tw("Atk",1)-8,               hdy, "Atk", 0x5050a0ff, 1);
            fr(plx, hdy+13, pw, 1, COL_BORDER);
            int ry2 = hdy + 15;
            vis = (LB_H - (ry2 - lb_y)) / row;
            if(c->server_lb_count == 0){
                dt(plx+6, ry2, "No games yet", 0x404060ff, 1);
            } else {
                int start = c->server_lb_scroll;
                if(start > c->server_lb_count - vis) start = c->server_lb_count - vis;
                if(start < 0) start = 0;
                for(i = start; i < c->server_lb_count && i < start+vis && i < SERVER_LB_MAX; i++){
                    ServerLbEntry *e = &c->server_lb[i];
                    uint32_t nc = i==0?COL_HOSTGOLD:i==1?COL_LTGRAY:i==2?0xcc8844ff:COL_GRAY;
                    fr(plx, ry2, pw, row-1, (i-start)%2?0x0e0e18ff:0x0b0b14ff);
                    {char rn[4];snprintf(rn,sizeof(rn),"%d",i+1); dt(hx+2,ry2+2,rn,0x404060ff,1);}
                    {char nk[18];strncpy(nk,e->nick,17);nk[17]='\0'; dt(hx+16,ry2+2,nk,nc,1);}
                    {char wn[8];snprintf(wn,sizeof(wn),"%d",e->wins);
                     dt(hx+pw-tw("Atk",1)-8-tw(wn,1)-8,ry2+2,wn,nc,1);}
                    {char an[8];snprintf(an,sizeof(an),"%d",e->attacks);
                     dt(hx+pw-tw(an,1)-6,ry2+2,an,0x6060a0ff,1);}
                    ry2+=row;
                }
                /* Scroll indicator */
                if(c->server_lb_count > vis){
                    char si[16];snprintf(si,sizeof(si),"%d/%d",start+1,c->server_lb_count);
                    dt(plx+pw-tw(si,1)-4,lb_y+LB_H-12,si,0x404060ff,1);}
            }
        }

        /* ---- TEAMS LEADERBOARD panel ---- */
        dpanel(tlx, lb_y, tw2, LB_H, "TEAMS LEADERBOARD");
        {
            int hx  = tlx+4;
            int hdy = lb_y+20;
            dt(hx+4,   hdy, "Team", 0x5050a0ff, 1);
            dt(hx+tw2-tw("W",1)-tw("Mbr",1)-20, hdy, "Mbr", 0x5050a0ff, 1);
            dt(hx+tw2-tw("W",1)-8,              hdy, "W",   0x5050a0ff, 1);
            fr(tlx, hdy+13, tw2, 1, COL_BORDER);
            int ry2 = hdy + 15;
            vis = (LB_H - (ry2 - lb_y)) / row;
            if(c->server_team_lb_count == 0){
                dt(tlx+6, ry2, "No teams yet", 0x404060ff, 1);
            } else {
                int start = c->server_team_lb_scroll;
                if(start > c->server_team_lb_count - vis) start = c->server_team_lb_count - vis;
                if(start < 0) start = 0;
                for(i = start; i < c->server_team_lb_count && i < start+vis && i < SERVER_LB_MAX; i++){
                    ServerTeamLbEntry *e = &c->server_team_lb[i];
                    uint32_t nc = i==0?COL_HOSTGOLD:i==1?COL_LTGRAY:i==2?0xcc8844ff:COL_GRAY;
                    fr(tlx, ry2, tw2, row-1, (i-start)%2?0x0e0e18ff:0x0b0b14ff);
                    {char rn[4];snprintf(rn,sizeof(rn),"%d",i+1); dt(hx+2,ry2+2,rn,0x404060ff,1);}
                    {char tn[14];strncpy(tn,e->team,13);tn[13]='\0'; dt(hx+16,ry2+2,tn,nc,1);}
                    {char mn[6];snprintf(mn,sizeof(mn),"%d",e->members);
                     dt(hx+tw2-tw("W",1)-tw(mn,1)-20,ry2+2,mn,0x6060a0ff,1);}
                    {char wn[8];snprintf(wn,sizeof(wn),"%d",e->wins);
                     dt(hx+tw2-tw(wn,1)-8,ry2+2,wn,nc,1);}
                    ry2+=row;
                }
                if(c->server_team_lb_count > vis){
                    char si[16];snprintf(si,sizeof(si),"%d/%d",start+1,c->server_team_lb_count);
                    dt(tlx+tw2-tw(si,1)-4,lb_y+LB_H-12,si,0x404060ff,1);}
            }
        }

        /* ---- PLAYERS ONLINE panel ---- */
        dpanel(olx, lb_y, ow, LB_H, "PLAYERS ONLINE");
        {
            int hx  = olx+4;
            int hdy = lb_y+20;
            /* Three columns: Nick | Room | Status
               Room column narrowed so Status isn't flush against the right border */
            int col_nick=hx+14;
            int col_room=hx+ow*55/100;   /* pushed right a bit less — was 5/12 ≈ 42% */
            int col_stat=hx+ow*75/100;   /* fixed offset, not pinned to right edge */
            dt(col_nick, hdy, "Nick",   0x5050a0ff, 1);
            dt(col_room, hdy, "Room",   0x5050a0ff, 1);
            dt(col_stat, hdy, "Status", 0x5050a0ff, 1);
            fr(olx, hdy+13, ow, 1, COL_BORDER);
            int ry2 = hdy + 15;
            vis = (LB_H - (ry2 - lb_y)) / row;
            if(c->online_player_count==0){
                dt(olx+6, ry2, "No players online", 0x404060ff, 1);
            } else {
                int start=0, k;
                for(k=start;k<c->online_player_count&&k<start+vis;k++){
                    OnlinePlayer *op=&c->online_players[k];
                    int playing=op->playing;
                    uint32_t nc=playing?COL_OPEN_GREEN:0x7070a0ff;
                    fr(olx, ry2, ow, row-1, k%2?0x0e0e18ff:0x0b0b14ff);
                    /* Nick */
                    {char nk[14];strncpy(nk,op->nick,13);nk[13]='\0';
                     dt(col_nick,ry2+2,nk,COL_LTGRAY,1);}
                    /* Room */
                    {char rm[10];
                     if(op->room_id>0) snprintf(rm,sizeof(rm),"Room %d",op->room_id);
                     else              strncpy(rm,"Lobby",sizeof(rm));
                     dt(col_room,ry2+2,rm,0x6060a0ff,1);}
                    /* Status */
                    dt(col_stat,ry2+2,
                       playing?"Playing":"Idle",nc,1);
                    ry2+=row;
                }
                if(c->online_player_count>vis){
                    char si[16];snprintf(si,sizeof(si),"%d total",c->online_player_count);
                    dt(olx+ow-tw(si,1)-4,lb_y+LB_H-12,si,0x404060ff,1);}
            }
        }

        /* ---- MOTD panel — between leaderboards and global chat ---- */
        {
            int motd_y = lb_y + LB_H + 6;
            int motd_h = 28;
            const char *motd = c->motd[0] ? c->motd : "Welcome to LineClash!";
            fr(list_x,motd_y,list_w,motd_h,0x0a0a1eff); dr(list_x,motd_y,list_w,motd_h,COL_PIECE_I);
            dt(list_x+6,motd_y+4,"MOTD:",COL_PIECE_I,1);
            dt(list_x+6+tw("MOTD: ",1),motd_y+4,motd,COL_LTGRAY,1);
            /* Admin: click to edit MOTD */
            if(c->priv_level<=2){
                if(dbtn(list_x+list_w-70,motd_y+4,66,20,"Edit",BS_NORMAL))
                    action=BTN_MOTD_EDIT;
            }

        /* ---- Global Chat panel — below MOTD, half the available height ---- */
        {
            int cy   = motd_y + motd_h + 4;
            int avail = WIN_H - cy - 10;
            int ch   = avail / 2;
            if(ch < 60) ch = 60;
            if(ch > 40){
                int lh=FH+3;
                int sb_w=8, input_h=20; /* input always visible for all users */
                int msg_area_h=ch-22-1-input_h; /* header+sep+input */
                int maxl=msg_area_h/lh; if(maxl<1)maxl=1;
                /* Clamp scroll */
                int max_scroll=c->schat_count-maxl; if(max_scroll<0)max_scroll=0;
                if(c->schat_scroll>max_scroll)c->schat_scroll=max_scroll;
                if(c->schat_scroll<0)c->schat_scroll=0;

                fr(list_x,cy,list_w,ch,0x0a120aff);
                dr(list_x,cy,list_w,ch,0x2a5a2aff);
                dt(list_x+4,cy+3,"SERVER CHAT",0x44aa44ff,1);
                {   int btn_w=tw("Chat (Tab)",1)+14;
                    if(dbtn(list_x+list_w-sb_w-btn_w-2,cy+2,btn_w,16,"Chat (Tab)",BS_NORMAL))
                        action=BTN_GLOBAL_CHAT;
                }
                fr(list_x,cy+20,list_w,1,0x1a3a1aff);

                /* Messages with scroll */
                {int end=c->schat_count-c->schat_scroll; if(end<0)end=0;
                 int start=end-maxl; if(start<0)start=0; int gi;
                 int msg_w=list_w-sb_w;
                 for(gi=start;gi<end;gi++){
                     int ty=cy+22+(gi-start)*lh;
                     uint32_t col=c->schat[gi].colour;
                     if(c->schat[gi].age>300)col=(col&0xffffff00)|0x80;
                     char cl[128]; int mc=(msg_w-8)/(FW+1); if(mc>127)mc=127;
                     strncpy(cl,c->schat[gi].text,mc);cl[mc]='\0';
                     dt(list_x+4,ty,cl,col,1);
                }}

                /* Scrollbar */
                {int sty=cy+22, sth=ch-22-input_h;
                 int sbx=list_x+list_w-sb_w;
                 fr(sbx,sty,sb_w,sth,0x061006ff);
                 if(max_scroll>0){
                     int th=sth*maxl/(c->schat_count>0?c->schat_count:1);
                     if(th<10)th=10;
                     int ty2=sty+(sth-th)*(max_scroll-c->schat_scroll)/max_scroll;
                     uint32_t thumb_col=c->schat_sb_drag?0x44aa44ff:0x2a6a2aff;
                     fr(sbx+1,ty2,sb_w-2,th,thumb_col);
                     /* Start drag on mousedown over thumb */
                     if(g_mouse.held&&pt_in(g_mouse.x,g_mouse.y,sbx+1,ty2,sb_w-2,th)
                        &&!c->schat_sb_drag){
                         c->schat_sb_drag=1;
                         c->schat_sb_drag_y0=g_mouse.y;
                         c->schat_sb_drag_scroll0=c->schat_scroll;
                     }
                     /* Continue drag */
                     if(c->schat_sb_drag&&g_mouse.held){
                         int dy=g_mouse.y-c->schat_sb_drag_y0;
                         int ds=dy*(max_scroll+1)/(sth-th+1);
                         c->schat_scroll=c->schat_sb_drag_scroll0-ds;
                         if(c->schat_scroll<0)c->schat_scroll=0;
                         if(c->schat_scroll>max_scroll)c->schat_scroll=max_scroll;
                     }
                     /* Release drag */
                     if(!g_mouse.held) c->schat_sb_drag=0;
                     /* Click track to jump */
                     if(pt_in(g_mouse.x,g_mouse.y,sbx,sty,sb_w,sth)&&g_mouse.clicked
                        &&!pt_in(g_mouse.x,g_mouse.y,sbx+1,ty2,sb_w-2,th)){
                         int rel=g_mouse.y-sty;
                         c->schat_scroll=max_scroll-(rel*max_scroll/sth);
                     }
                 } else {
                     if(!g_mouse.held) c->schat_sb_drag=0;
                 }}

                /* Input box — visible for all users */
                {   int iy=cy+ch-input_h;
                    fr(list_x,iy-1,list_w,1,0x1a4a1aff);
                    uint32_t ibg=c->schat_input.active?0x061606ff:0x040c04ff;
                    uint32_t ibo=c->schat_input.active?0x44cc44ff:0x1a4a1aff;
                    fr(list_x,iy,list_w,input_h,ibg);
                    dr(list_x,iy,list_w,input_h,ibo);
                    if(c->schat_input.active){
                        char disp[256]; snprintf(disp,sizeof(disp),"> %s_",c->schat_input.text);
                        dt(list_x+4,iy+(input_h-FH)/2,disp,COL_OPEN_GREEN,1);
                    } else {
                        dt(list_x+4,iy+(input_h-FH)/2,"Tab or click to chat...",0x2a5a2aff,1);
                    }
                    if(pt_in(g_mouse.x,g_mouse.y,list_x,cy,list_w,ch)
                       &&g_mouse.clicked&&!c->schat_input.active)
                        action=BTN_GLOBAL_CHAT;
                }
            }
        }
        }
        (void)hdr_rows;
    }

    g_popup_blocked = c->motd_popup_open || c->keymap_popup_open || c->about_popup_open || c->music_player_open;
    int saved_click_ls = g_mouse.clicked; /* capture before popup blocks it */
    if(g_popup_blocked) g_mouse.clicked = 0;
    /* MOTD edit popup */
    if(c->motd_popup_open){
        int pw=560,ph=120,px=(WIN_W-pw)/2,py=(WIN_H-ph)/2;
        int saved_click_motd=saved_click_ls;
        SDL_SetRenderDrawBlendMode(g_rend,SDL_BLENDMODE_BLEND);
        fr(0,0,WIN_W,WIN_H,0x000000b0);
        SDL_SetRenderDrawBlendMode(g_rend,SDL_BLENDMODE_NONE);
        if(saved_click_motd&&!pt_in(g_mouse.x,g_mouse.y,px,py,pw,ph)){
            c->motd_popup_open=0; g_mouse.clicked=0; }
        else {
        fr(px,py,pw,ph,0x10102aff);
        fr(px,py,pw,28,0x1a1a40ff);
        dr(px,py,pw,ph,COL_PIECE_I);
        dtc(px+pw/2,py+9,"Edit MOTD",COL_LTGRAY,1);
        dt(px+14,py+34,"Message of the Day:",0x4040a0ff,1);
        {int foc=c->tf_motd.focused, fw2=pw-28;
         fr(px+14,py+48,fw2,26,foc?0x1a1a38ff:0x0e0e1eff);
         dr(px+14,py+48,fw2,26,foc?0x7070c0ff:0x222244ff);
         if(pt_in(g_mouse.x,g_mouse.y,px+14,py+48,fw2,26)&&g_mouse.clicked) c->tf_motd.focused=1;
         {char disp[130];snprintf(disp,sizeof(disp),"%s%s",c->tf_motd.text,foc?"_":"");
          dt(px+18,py+54,c->tf_motd.len>0?disp:"(enter motd)",
             c->tf_motd.len>0?COL_WHITE:0x404060ff,1);}
        }
        {int bby=py+ph-40,bw2=(pw-42)/2;
         if(dbtn(px+14,bby,bw2,32,"Save",BS_ACTIVE)) action=BTN_MOTD_POPUP_OK;
         if(dbtn(px+28+bw2,bby,bw2,32,"Cancel",BS_NORMAL)) c->motd_popup_open=0;
        }
        } /* end else (popup content) */
    }

    /* ── ABOUT POPUP (lobby-select) ── */
    if(c->about_popup_open) { render_about_popup(c, saved_click_ls); }

    if((c->state==CS_PLAYING || c->state==CS_GAMEOVER) && c->music_player_open) render_music_popup(c,saved_click_ls);

    /* ── KEYMAP POPUP (lobby-select) ── */
    if(c->keymap_popup_open) { render_keymap_popup(c, saved_click_ls); }

    if(action!=BTN_NONE) client_on_button(c,action);
    SDL_RenderPresent(g_rend);
}

/* ============================== GAME SCREEN ============================== */
void sdl_render_game(ClientCtx*c){
    int any_popup_g = (c->kick_popup_open||c->team_popup_open||
                       c->game_options_popup_open||c->ai_players_popup_open||
                       c->keymap_popup_open||c->about_popup_open||c->music_player_open);
    int saved_click_g = g_mouse.clicked;
    g_popup_blocked = any_popup_g;
    if(any_popup_g) g_mouse.clicked=0;
    g_rtick++;
    int i,opp_x,opp_y;
    int fx=OWN_FIELD_X,fy=OWN_FIELD_Y;
    int fw2=FIELD_WIDTH*BLOCK_SIZE,fh2=FIELD_HEIGHT*BLOCK_SIZE;
    int sbx=SIDEBAR_X,sbw=SIDEBAR_W;
    int lby,lb_h=0;
    char buf[80];
    BtnId action=BTN_NONE;

    SDL_SetRenderDrawColor(g_rend,0x0b,0x11,0x1b,0xff);
    SDL_RenderClear(g_rend);

    /* Header */
    fr(0,0,WIN_W,28,0x1c2437ff);fr(0,27,WIN_W,1,0x3b4864ff);
    /* Back to lobby button — top left */
    if(dbtn(4,3,120,22,"BACK TO LOBBY",BS_NORMAL)) action=BTN_BACK_TO_LOBBIES;
    {char base[64];
     snprintf(base,sizeof(base),"Room %d  |  %s",c->room_id,c->nick);
     dt(132,8,base,COL_LTGRAY,1);
     {int bx=132+tw(base,1);
      /* All role/privilege/spectator badges in gold */
      const char *role = c->is_host ? " [HOST]" : " [PLAYER]";
      dt(bx,8,role,COL_HOSTGOLD,1); bx+=tw(role,1);
      if(c->priv_level==1)   { dt(bx,8," [ADM]",COL_HOSTGOLD,1); bx+=tw(" [ADM]",1); }
      else if(c->priv_level==2){ dt(bx,8," [MGR]",COL_HOSTGOLD,1); bx+=tw(" [MGR]",1); }
      else if(c->priv_level==3){ dt(bx,8," [USR]",COL_HOSTGOLD,1); bx+=tw(" [USR]",1); }
      if(c->is_spectator)    { dt(bx,8," [SPECTATOR]",COL_HOSTGOLD,1); }
     }}
    /* Header right side: ping far-right, score/browse to its left */
    {
        /* Build ping string and measure it */
        char ping_str[24]; uint32_t ping_col;
        if (c->ping_ms < 0)
            { snprintf(ping_str,sizeof(ping_str),"PING: --"); ping_col=0x606060ff; }
        else if (c->ping_ms < 60)
            { snprintf(ping_str,sizeof(ping_str),"PING: %dms",c->ping_ms); ping_col=0x40ff80ff; }
        else if (c->ping_ms < 150)
            { snprintf(ping_str,sizeof(ping_str),"PING: %dms",c->ping_ms); ping_col=0xffcc40ff; }
        else
            { snprintf(ping_str,sizeof(ping_str),"PING: %dms",c->ping_ms); ping_col=0xff4040ff; }
        int ping_w = tw(ping_str,1);
        int ping_x = WIN_W - ping_w - 8;
        dt(ping_x, 8, ping_str, ping_col, 1);

        /* Score info left of ping with 12px gap */
        int score_rx = ping_x - 12;
        if(!c->is_spectator){
            snprintf(buf,sizeof(buf),"Lvl:%d  Lines:%d  Score:%d",
                     c->my_field.level, c->my_field.lines, c->my_field.score);
            dt(score_rx - tw(buf,1), 8, buf, COL_OPEN_GREEN, 1);
        } else {
            const char *hint = "<- -> to browse";
            dt(score_rx - tw(hint,1), 8, hint, 0x6060a0ff, 1);
        }
    }

    /* Own field — or watched player's field when spectating/eliminated */
    /* Own field header — identical style to opponent boards (dopp) */
    if(!((c->is_spectator && c->state==CS_PLAYING) || c->state==CS_GAMEOVER)){
        /* Panel border background */
        fr(fx-2,fy-16,fw2+4,fh2+20,COL_HIGHLIGHT);
        dr(fx-2,fy-16,fw2+4,fh2+20,COL_BORDER);
        /* Slot chip: always "1" — gold, left side */
        fr(fx-2,fy-16,FW+6,16,0x1a1a00ff);
        dt(fx,fy-14,"1",COL_HOSTGOLD,1);
        /* Nick */
        {int nick_x=fx+FW+5;
         dt(nick_x,fy-14,c->nick,COL_WHITE,1);}
        /* HOST / ADMIN badges — right side */
        if(c->priv_level<=2){
            int bw=tw("[ADMIN]",1)+2;
            dt(fx+fw2-bw,fy-14,"[ADMIN]",COL_HOSTGOLD,1);
        } else if(c->is_host){
            int bw=tw("HOST",1)+2;
            dt(fx+fw2-bw,fy-14,"HOST",COL_HOSTGOLD,1);
        }
    }
    /* Target ring on own field when self is selected as attack target */
    {uint32_t own_border = (c->attack_target==c->slot && c->state==CS_PLAYING
                            && !c->paused && !c->is_spectator)
                           ? 0xffff00ff : 0x5050a0ff;
     dr(fx-2,fy-2,fw2+4,fh2+4,own_border);}
    {
    int can_browse = (c->is_spectator && c->state==CS_PLAYING) || c->state==CS_GAMEOVER;
    if(can_browse){
        /* Find a valid slot to display */
        int ws = c->spec_watch_slot;
        if(ws<=0||ws>MAX_PLAYERS||!c->opp_nicks[ws-1][0]){
            int fi;
            for(fi=0;fi<MAX_PLAYERS;fi++){
                if(fi+1!=c->slot&&c->opp_nicks[fi][0]){ws=fi+1;break;}
            }
            c->spec_watch_slot=ws;
        }
        if(ws>=1&&ws<=MAX_PLAYERS&&c->opp_nicks[ws-1][0]){
            Field *wf=&c->opp_fields[ws-1];
            dfield(fx,fy,BLOCK_SIZE,wf,NULL,0);
            fr(fx,fy-18,fw2,18,0x0a0a1eff);
            {char wb[40];snprintf(wb,sizeof(wb),"%s: %s",
                c->state==CS_GAMEOVER?"SPECTATING":"Watching",c->opp_nicks[ws-1]);
             dtc(fx+fw2/2,fy-15,wb,c->state==CS_GAMEOVER?COL_INGAME_RED:0xaaaaffff,1);}
            if(!c->opp_alive[ws-1]){
                SDL_SetRenderDrawBlendMode(g_rend,SDL_BLENDMODE_BLEND);
                fr(fx,fy,fw2,fh2,0x00000088);
                SDL_SetRenderDrawBlendMode(g_rend,SDL_BLENDMODE_NONE);
                dtc(fx+fw2/2,fy+fh2/2-6,"ELIMINATED",COL_INGAME_RED,1);}
            dtc(fx+8,fy+fh2/2,"<",COL_LTGRAY,2);
            dtc(fx+fw2-8,fy+fh2/2,">",COL_LTGRAY,2);
            if(g_mouse.clicked){
                if(pt_in(g_mouse.x,g_mouse.y,fx,fy,fw2/2,fh2)){
                    int s=c->spec_watch_slot,tries;
                    for(tries=0;tries<MAX_PLAYERS;tries++){
                        s=((s-2+MAX_PLAYERS)%MAX_PLAYERS)+1;
                        if(s!=c->slot&&c->opp_nicks[s-1][0]){c->spec_watch_slot=s;break;}
                    }
                } else if(pt_in(g_mouse.x,g_mouse.y,fx+fw2/2,fy,fw2/2,fh2)){
                    int s=c->spec_watch_slot,tries;
                    for(tries=0;tries<MAX_PLAYERS;tries++){
                        s=(s%MAX_PLAYERS)+1;
                        if(s!=c->slot&&c->opp_nicks[s-1][0]){c->spec_watch_slot=s;break;}
                    }
                }
            }
        } else {
            fr(fx,fy,fw2,fh2,0x0a0a14ff);
            dtc(fx+fw2/2,fy+fh2/2,"No active players",COL_GRAY,1);
        }
    } else {
        dfield(fx,fy,BLOCK_SIZE,&c->my_field,
               (c->state==CS_PLAYING&&!c->paused)?&c->cur:NULL,
               (c->state==CS_PLAYING&&!c->paused));
        /* ── Pending garbage meter — left of field, guideline mode only ── */
        if(c->state==CS_PLAYING && !c->paused && c->garbage_system==1 && c->pending_garbage>0){
            int mg=c->pending_garbage; if(mg>FIELD_HEIGHT) mg=FIELD_HEIGHT;
            int mw=6, mh=FIELD_HEIGHT*BLOCK_SIZE;
            int mx=fx-mw-3, my2=fy;
            /* Background track */
            fr(mx,my2,mw,mh,0x0a0a14ff); dr(mx,my2,mw,mh,0x202030ff);
            /* Filled portion — red, grows from bottom */
            int fill_h = mg * BLOCK_SIZE;
            /* Pulse intensity */
            int pp=(int)(g_rtick%30);
            uint8_t pal2=(uint8_t)(pp<15?160+pp*4:220-(pp-15)*4);
            uint32_t gc=(mg>=8)?((uint32_t)(0xff0000ff|(pal2))): /* danger: bright red */
                         (mg>=4)?0xff4400ff:0xff8800ff;  /* warning: orange; caution: amber */
            /* override with pulse alpha */
            gc=(gc&0xffffff00)|pal2;
            fr(mx, my2+mh-fill_h, mw, fill_h, gc);
            dr(mx,my2,mw,mh,0x804040ff);
            /* Count label */
            if(mg>=1){char gc2[4];snprintf(gc2,sizeof(gc2),"%d",mg);
             dt(mx+(mw-FW)/2-1, my2+mh-fill_h-FH-2, gc2, 0xff6060ff, 1);}
        }
        /* Highlight special blocks in own field with animated glow */
        if(c->state==CS_PLAYING) {
            int bsz = BLOCK_SIZE;
            /* Animate: pulse border alpha 80..ff over 60 frames */
            int ph = (int)(g_rtick % 60);
            uint8_t pal = (uint8_t)(ph < 30 ? 128 + ph*4 : 248 - (ph-30)*4);
            for(int sy=0;sy<FIELD_HEIGHT;sy++) for(int sx2=0;sx2<FIELD_WIDTH;sx2++) {
                uint8_t v=c->my_field.cells[sy][sx2];
                if(v>8&&v<=8+NUM_SPECIALS) {
                    uint32_t bc = (SC[v-8]&0xffffff00)|pal;
                    dr(fx+sx2*bsz-1, fy+sy*bsz-1, bsz+2, bsz+2, bc);
                }
            }
        }
    }
    } /* end can_browse block */

    /* Paused overlay — show for all players; only non-spectators can resume */
    if(c->paused){
        SDL_SetRenderDrawBlendMode(g_rend,SDL_BLENDMODE_BLEND);
        fr(fx,fy,fw2,fh2,0x00000099);
        SDL_SetRenderDrawBlendMode(g_rend,SDL_BLENDMODE_NONE);
        dtc(fx+fw2/2,fy+fh2/2-14,"PAUSED",COL_HOSTGOLD,2);
        if(!c->is_spectator)
            if(dbtn(fx+fw2/2-50,fy+fh2/2+10,100,28,"RESUME",BS_ACTIVE)) action=BTN_RESUME;
    }

    /* Status effect overlays (darkness, confusion, immunity, mutate) */
    if(!c->is_spectator && c->state==CS_PLAYING){
        if(c->darkness_timer>0){
            int vis_rows=3;
            SDL_SetRenderDrawBlendMode(g_rend,SDL_BLENDMODE_BLEND);
            fr(fx,fy,fw2,fh2-(vis_rows*BLOCK_SIZE),0x000000ee);
            SDL_SetRenderDrawBlendMode(g_rend,SDL_BLENDMODE_NONE);
            dtc(fx+fw2/2,fy+10,"DARKNESS",0x8060ffff,1);
            dtc(fx+fw2/2,fy+22,"(limited vision)",0x5040aaff,1);
        }
        if(c->confusion_timer>0){
            SDL_SetRenderDrawBlendMode(g_rend,SDL_BLENDMODE_BLEND);
            fr(fx,fy,fw2,10,0xff60a033);
            SDL_SetRenderDrawBlendMode(g_rend,SDL_BLENDMODE_NONE);
            dtc(fx+fw2/2,fy+1,"CONFUSED",0xff60a0ff,1);
        }
        if(c->immunity_timer>0){
            SDL_SetRenderDrawBlendMode(g_rend,SDL_BLENDMODE_BLEND);
            dr(fx-2,fy-2,fw2+4,fh2+4,0x40ffcc88);
            dr(fx-3,fy-3,fw2+6,fh2+6,0x40ffcc44);
            SDL_SetRenderDrawBlendMode(g_rend,SDL_BLENDMODE_NONE);
            dtc(fx+fw2/2,fy+1,"IMMUNE",0x40ffccff,1);
        }
        if(c->mutate_timer>0){
            dtc(fx+fw2/2,fy+fh2-12,"MUTATED",0xffaa40ff,1);
        }
    }

    /* ---- Attack bar — full window width below own field ---- */
    {
        BtnId ab=draw_attack_bar(0,ATTACK_BAR_Y,WIN_W,ATTACK_BAR_H,c);
        if(ab!=BTN_NONE) action=ab;
    }

    /* ---- Sidebar ---- */
    /* Hold piece */
    {/* Build hold key hint from keymap */
     char _hk[12];
     int _hsc=c->km.hold;
     if(_hsc==6) snprintf(_hk,sizeof(_hk),"[C]");
     else if(_hsc==225||_hsc==229) snprintf(_hk,sizeof(_hk),"[Sft]");
     else if(_hsc>=4&&_hsc<=29) snprintf(_hk,sizeof(_hk),"[%c]",(char)('A'+_hsc-4));
     else snprintf(_hk,sizeof(_hk),"[%d]",_hsc);
     dhold(sbx, 36, sbw, 70, c->hold.type, c->hold_used, _hk);}
    /* Next piece */
    dnext(sbx,114,sbw,70,&c->next);
    if((c->state==CS_PLAYING || c->state==CS_GAMEOVER) && dbtn(sbx,190,116,26,"Music Player",BS_NORMAL)) action=BTN_MUSIC_PLAYER;
    if(dbtn(sbx+122,190,sbw-122,26,"About",BS_NORMAL)) action=BTN_SRV_ABOUT;

    /* PLAYER WINS — per-room session leaderboard */
    {int py=224, ph=c->lb_count>0?(c->lb_count*16+22):36;
     int row_h=16, i;
     dpanel(sbx,py,sbw,ph,"PLAYER WINS");
     if(c->lb_count==0){
         dt(sbx+4,py+20,"No games yet",0x404060ff,1);
     } else {
         for(i=0;i<c->lb_count&&i<7;i++){
             int ry=py+18+i*row_h;
             uint32_t col=(i==0)?COL_HOSTGOLD:(i==1)?COL_LTGRAY:(i==2)?0xcc8844ff:COL_GRAY;
             {char rank[4];snprintf(rank,sizeof(rank),"%d.",i+1);dt(sbx+3,ry,rank,col,1);}
             {char nick[12];strncpy(nick,c->leaderboard[i].nick,11);nick[11]=0;dt(sbx+20,ry,nick,col,1);}
             {char wins[16];
              if(c->leaderboard[i].streak>=2)
                  snprintf(wins,sizeof(wins),"%d 🔥%d",c->leaderboard[i].wins,c->leaderboard[i].streak);
              else
                  snprintf(wins,sizeof(wins),"%d",c->leaderboard[i].wins);
              dt(sbx+sbw-tw(wins,1)-4,ry,wins,c->leaderboard[i].streak>=2?0xff8800ff:col,1);}
         }
     }
     lby=py+ph+6;}

    /* TEAM WINS — per-room session team leaderboard */
    {int ph=c->team_lb_count>0?(c->team_lb_count*16+22):36;
     int row_h=16, i;
     dpanel(sbx,lby,sbw,ph,"TEAM WINS");
     if(c->team_lb_count==0){
         dt(sbx+4,lby+20,"No team games yet",0x404060ff,1);
     } else {
         for(i=0;i<c->team_lb_count&&i<6;i++){
             int ry=lby+18+i*row_h;
             uint32_t col=(i==0)?COL_HOSTGOLD:(i==1)?COL_LTGRAY:(i==2)?0xcc8844ff:COL_GRAY;
             {char rank[4];snprintf(rank,sizeof(rank),"%d.",i+1);dt(sbx+3,ry,rank,col,1);}
             {char team[12];strncpy(team,c->team_leaderboard[i].nick,11);team[11]=0;dt(sbx+20,ry,team,col,1);}
             {char wins[6];snprintf(wins,sizeof(wins),"%d",c->team_leaderboard[i].wins);
              dt(sbx+sbw-tw(wins,1)-4,ry,wins,col,1);}
         }
     }
     lb_h=ph; lby=lby+ph;}


    /* ---- READY / ACTIONS section ---- */
    {
        int by=lby+6;

        if(c->state==CS_IN_LOBBY){
            /* Ready roster */
            {int ri;
             fr(sbx,by,sbw,1,COL_BORDER); by+=4;
             {uint32_t rc=c->is_ready?COL_OPEN_GREEN:0x606080ff;
              dt(sbx+4,by,c->is_ready?"[RDY]":"[   ]",rc,1);
              dt(sbx+46,by,c->nick,COL_LTGRAY,1);
              if(c->is_host) dt(sbx+sbw-tw("HOST",1)-6,by,"HOST",COL_HOSTGOLD,1);}
             by+=14;
             for(ri=0;ri<MAX_PLAYERS;ri++){
                 if(ri+1==c->slot||!c->opp_nicks[ri][0]) continue;
                 uint32_t rc=c->opp_ready[ri]?COL_OPEN_GREEN:0x606080ff;
                 dt(sbx+4,by,c->opp_ready[ri]?"[RDY]":"[   ]",rc,1);
                 dt(sbx+46,by,c->opp_nicks[ri],COL_LTGRAY,1);
                 by+=14;}
             by+=6;}

            /* START GAME (host only) — top of stack */
            if(c->is_host){
                int all_ready=c->is_ready, ri;
                for(ri=0;ri<MAX_PLAYERS;ri++){
                    if(ri+1==c->slot||!c->opp_nicks[ri][0]) continue;
                    if(!c->opp_ready[ri]){all_ready=0;break;}}
                BtnStyle sbs=all_ready?BS_ACTIVE:BS_DISABLED;
                if(dbtn(sbx+4,by,sbw-8,26,"START GAME",sbs)&&all_ready) action=BTN_START_GAME;
                by+=30;}

            /* READY button */
            {BtnStyle rbs=c->is_ready?BS_TOGON:BS_NORMAL;
             if(dbtn(sbx+4,by,sbw-8,26,"READY",rbs)&&!c->is_spectator) action=BTN_READY;
             by+=30;}

            /* GAME OPTIONS button */
            {BtnStyle gobs=(c->is_host||c->priv_level<=2)?BS_ACTIVE:BS_NORMAL;
             if(dbtn(sbx+4,by,sbw-8,26,"Game Options",gobs)) action=BTN_GAME_OPTIONS;
             by+=30;}

            /* TEAM button */
            {char tlbl[32];
             snprintf(tlbl,sizeof(tlbl),"Team: %s",c->team[0]?c->team:"(none)");
             if(dbtn(sbx+4,by,sbw-8,26,tlbl,BS_NORMAL)) action=BTN_TEAM_POPUP;
             by+=30;}

            /* AI PLAYERS button (host/admin only) */
            if(c->bots_allowed&&(c->is_host||c->priv_level<=2)){
                if(dbtn(sbx+4,by,sbw-8,26,"AI Players",BS_NORMAL)) action=BTN_AI_PLAYERS;
                by+=30;}
        }

        if(c->state==CS_GAMEOVER){
            fr(sbx,lby+lb_h+6,sbw,1,COL_BORDER);
            int wy=lby+lb_h+14;
            dt(sbx+4,wy,"Eliminated",0xff6060ff,1); wy+=14;
            dt(sbx+4,wy,"Waiting for round to",COL_GRAY,1); wy+=13;
            dt(sbx+4,wy,"end...",COL_GRAY,1);
        }
        if(c->state==CS_PLAYING&&!c->is_spectator){
            if(c->paused){
                if(dbtn(sbx+4,by,sbw-8,26,"RESUME",BS_ACTIVE)) action=BTN_RESUME;
            } else {
                if(dbtn(sbx+4,by,sbw-8,26,"Pause",BS_NORMAL)) action=BTN_PAUSE;}
            by+=30;
            /* END GAME — only for host, admin, or priv≤2 */
            if(c->is_host||c->priv_level<=2){
                if(dbtn(sbx+4,by,sbw-8,24,"END GAME",BS_NORMAL)) action=BTN_END_GAME;
                by+=28;
            }
        }
    }

    /* A quiet arena panel replaces the unstructured blank space in solo rooms. */
    {int opponents=0;
     for(int n=0;n<MAX_PLAYERS;n++)if(n+1!=c->slot && c->opp_nicks[n][0])opponents++;
     if(!opponents) {
         int ax=OPP_START_X,aw=WIN_W-ax-18;
         surface(ax,48,aw,260,0x131f2eff,0x2b3c51ff);
         dt(ax+24,72,"THE ARENA",0x72dfbbff,1);
         dt(ax+24,111,c->state==CS_IN_LOBBY?"Make room for a rivalry.":"Your board. Your rhythm.",0xe9f1fcff,2);
         dt(ax+24,163,"Opponent boards appear here when players join.",0x97abc5ff,1);
         dt(ax+24,190,"Practise solo, or invite someone into the room.",0x97abc5ff,1);
         fr(ax+24,223,aw-48,1,0x2b3c51ff);
         dt(ax+24,247,"F8  Previous track    F9  Pause music    F10  Next",0x97abc5ff,1);
     }}

    /* ---- Opponents with per-player TAUNT button below each board ---- */
    /* OPP_FIELD_H+18 = board panel height; +16 taunt btn gap; row pitch = OPP_FIELD_H+34+taunt */
    {
        int taunt_btn_h = (!c->no_taunts && c->state==CS_PLAYING && !c->is_spectator) ? 16 : 0;
        int row_pitch   = OPP_FIELD_H + 34 + taunt_btn_h;
        opp_x=OPP_START_X; opp_y=OWN_FIELD_Y+16;
        for(i=0;i<MAX_PLAYERS;i++){
            if(i+1==c->slot||!c->opp_nicks[i][0]) continue;
            dopp(opp_x,opp_y,&c->opp_fields[i],c->opp_nicks[i],c->opp_alive[i],c->opp_spectator[i],display_id(c,i+1));
            /* Target ring when this opponent is the attack target */
            if(c->attack_target==i+1 && c->state==CS_PLAYING && !c->paused && !c->is_spectator){
                dr(opp_x-3,opp_y-18,OPP_FIELD_W+6,OPP_FIELD_H+22,0xffff00ff);
                dr(opp_x-2,opp_y-17,OPP_FIELD_W+4,OPP_FIELD_H+20,0xffcc0080);}
            /* Click opponent board to watch — works for both spectators and eliminated players */
            {int can_watch=(c->is_spectator||c->state==CS_GAMEOVER);
             int is_watched=(can_watch&&c->spec_watch_slot==i+1);
             if(is_watched){
                 fr(opp_x-3,opp_y-18,OPP_FIELD_W+6,OPP_FIELD_H+22,0x2a2a50ff);
                 dr(opp_x-3,opp_y-18,OPP_FIELD_W+6,OPP_FIELD_H+22,0x8080ffff);}
             if(can_watch&&g_mouse.clicked&&
                pt_in(g_mouse.x,g_mouse.y,opp_x-3,opp_y-18,OPP_FIELD_W+6,OPP_FIELD_H+22))
                 c->spec_watch_slot=i+1;
            }

            /* TAUNT button — disabled for spectators and when paused */
            if(taunt_btn_h>0){
                int tby  = opp_y + OPP_FIELD_H + 4;
                int is_spec = c->opp_spectator[i];
                int can  = (!is_spec && c->taunt_cooldown==0 && !c->paused);
                int hov  = can && pt_in(g_mouse.x,g_mouse.y,opp_x-2,tby,OPP_FIELD_W+4,12);
                fr(opp_x-2,tby,OPP_FIELD_W+4,12,hov?0x3a1000ff:0x160800ff);
                dr(opp_x-2,tby,OPP_FIELD_W+4,12,can?0x884400ff:0x2a1a0aff);
                const char *tlbl = is_spec?"SPECTATOR":c->paused?"PAUSED":"TAUNT";
                uint32_t tcol = is_spec?0x4040a0ff:c->paused?COL_HOSTGOLD:can?0xffaa00ff:0x443320ff;
                dtc(opp_x+OPP_FIELD_W/2,tby+1,tlbl,tcol,1);
                if(hov&&g_mouse.clicked){
                    char tbuf[MAX_MSG_LEN];
                    snprintf(tbuf,sizeof(tbuf),"taunt %s",c->opp_nicks[i]);
                    net_send(c->sock,tbuf); c->taunt_cooldown=300;}
            }

            opp_x+=OPP_FIELD_W+14;
            if(opp_x+OPP_FIELD_W>WIN_W-4){opp_x=OPP_START_X; opp_y+=row_pitch;}
        }
    }

    /* ---- Chat + Activity panel side by side ---- */
    {
        int cy  = CHAT_Y;
        int cw  = WIN_W*6/10;
        int aw  = WIN_W - cw - 2;
        if(draw_chat_panel(0,cy,cw,CHAT_H,c)) action=BTN_OPEN_CHAT;
        draw_activity_log(cw+2,cy,aw,CHAT_H,c);
    }

    /* Status strip */
    fr(0,WIN_H-22,WIN_W,22,0x0a0a1aff);dr(0,WIN_H-22,WIN_W,22,COL_BORDER);
    if(c->state==CS_IN_LOBBY){
         dt(4,WIN_H-17,c->is_host?"HOST -- click START":"Waiting for host",
            c->is_host?COL_HOSTGOLD:COL_LTGRAY,1);
    } else if(c->state==CS_PLAYING){
         if(c->is_spectator)      dt(4,WIN_H-17,"SPECTATING",0xaaaaffff,1);
         else if(c->paused)       dt(4,WIN_H-17,"PAUSED",COL_HOSTGOLD,1);
         else                     dt(4,WIN_H-17,"PLAYING",COL_OPEN_GREEN,1);
    } else if(c->state==CS_GAMEOVER){
         dt(4,WIN_H-17,"ELIMINATED",COL_INGAME_RED,1);
    }

    draw_winner(c);

    /* ---- Team name popup overlay ---- */
    /* ---- Kick / Ban notification popup ---- */
    if(c->kick_popup_open){
        int pw=380,ph=140,px=(WIN_W-pw)/2,py=(WIN_H-ph)/2;
        SDL_SetRenderDrawBlendMode(g_rend,SDL_BLENDMODE_BLEND);
        fr(0,0,WIN_W,WIN_H,0x00000099);
        SDL_SetRenderDrawBlendMode(g_rend,SDL_BLENDMODE_NONE);
        g_mouse.clicked=saved_click_g;
        g_popup_blocked=0; /* popup content is interactive */
        /* box: red=ban, orange=kick, blue=admin disconnect */
        int _is_admin_kill=(strstr(c->kick_popup_msg,"administrator")!=NULL);
        uint32_t border_col=_is_admin_kill?0x4488ffff:
                            c->kick_popup_is_ban?0xff3333ff:0xff8800ff;
        fr(px,py,pw,ph,0x08101aff);
        dr(px,py,pw,ph,border_col);
        /* title */
        const char *title=_is_admin_kill?"Connection Terminated":
                          c->kick_popup_is_ban?"You have been banned":"You have been kicked";
        dtc(px+pw/2,py+10,title,border_col,1);
        /* horizontal rule */
        fr(px+8,py+26,pw-16,1,border_col);
        /* reason — scale 2 so it's unmissable, wrap at 22 chars */
        {int mc=22; char ln1[64]="",ln2[64]="";
         int rlen=(int)strlen(c->kick_popup_msg);
         if(rlen==0){
             dtc(px+pw/2,py+48,"(no reason given)",0xaaaaaaff,1);
         } else if(rlen<=mc){
             dtc(px+pw/2,py+46,c->kick_popup_msg,0xffee88ff,2);
         } else {
             int sp=mc;
             while(sp>0&&c->kick_popup_msg[sp]!=' ') sp--;
             if(!sp) sp=mc;
             strncpy(ln1,c->kick_popup_msg,sp); ln1[sp]='\0';
             strncpy(ln2,c->kick_popup_msg+sp+1,sizeof(ln2)-1);
             dtc(px+pw/2,py+40,ln1,0xffee88ff,2);
             dtc(px+pw/2,py+60,ln2,0xffee88ff,2);
         }}
        /* OK button — no hint text */
        if(dbtn(px+(pw-80)/2,py+ph-38,80,26,"OK",BS_ACTIVE)) action=BTN_KICK_POPUP_OK;
    }

    if(c->team_popup_open){
        int pw=320,ph=100,px=(WIN_W-pw)/2,py=(WIN_H-ph)/2;
        SDL_SetRenderDrawBlendMode(g_rend,SDL_BLENDMODE_BLEND);
        fr(0,0,WIN_W,WIN_H,0x00000088);
        SDL_SetRenderDrawBlendMode(g_rend,SDL_BLENDMODE_NONE);
        g_mouse.clicked=saved_click_g;
        if(saved_click_g&&!pt_in(g_mouse.x,g_mouse.y,px,py,pw,ph)){
            c->team_popup_open=0; g_mouse.clicked=0; }
        else {
        g_popup_blocked=0;
        fr(px,py,pw,ph,0x14142aff); dr(px,py,pw,ph,COL_PIECE_I);
        dtc(px+pw/2,py+10,"Set Team Name",COL_LTGRAY,1);
        /* text field */
        {int fx2=px+8,fw2=pw-16,fh2=24;
         fr(fx2,py+28,fw2,fh2,0x1e1e3aff); dr(fx2,py+28,fw2,fh2,0x8080c0ff);
         {char disp[130];
          snprintf(disp,sizeof(disp),"%s_",c->tf_team_popup.text);
          dt(fx2+4,py+34,disp,COL_WHITE,1);}}
        /* buttons */
        if(dbtn(px+8,py+64,(pw-24)/2,26,"Set",BS_ACTIVE))    action=BTN_TEAM_POPUP_SET;
        if(dbtn(px+16+(pw-24)/2,py+64,(pw-24)/2,26,"Cancel",BS_NORMAL)) c->team_popup_open=0;
        /* hint */
        dtc(px+pw/2,py+ph-8,"Enter = set  |  Esc = cancel",0x404060ff,1);
        } /* end else */
    }

    /* ── GAME OPTIONS POPUP ── */
    if(c->game_options_popup_open){
        int pw=380, ph=394;
        int px=(WIN_W-pw)/2, py=(WIN_H-ph)/2;
        int can_edit=(c->is_host||c->priv_level<=2);
        /* Read-only table is more compact — shrink popup height for non-editors */
        if(!can_edit){ ph=320; py=(WIN_H-ph)/2; }
        SDL_SetRenderDrawBlendMode(g_rend,SDL_BLENDMODE_BLEND);
        fr(0,0,WIN_W,WIN_H,0x00000099);
        SDL_SetRenderDrawBlendMode(g_rend,SDL_BLENDMODE_NONE);
        g_mouse.clicked=saved_click_g;
        if(g_mouse.clicked&&!pt_in(g_mouse.x,g_mouse.y,px,py,pw,ph)){
            c->game_options_popup_open=0; g_mouse.clicked=0; }
        else {
        g_popup_blocked=0; /* enable hover/click inside popup */
        fr(px,py,pw,ph,0x10102aff); dr(px,py,pw,ph,COL_PIECE_I);
        dtc(px+pw/2,py+10,"Game Options",COL_LTGRAY,2);
        if(can_edit){
        /* ── Interactive toggle buttons for host/admin ── */
        #define OPT_ROW(label,state_on,sc_on,sc_off,btn_id,row_y) \
        { int on=(state_on); uint32_t sc=on?(sc_on):(sc_off); \
          int bx=px+12,by2=(row_y),bw2=pw-24,bh2=36; \
          int hv=pt_in(g_mouse.x,g_mouse.y,bx,by2,bw2,bh2); \
          fr(bx,by2,bw2,bh2,on?0x061006ff:0x160606ff); \
          if(hv) fr(bx,by2,bw2,bh2,0xffffff18); \
          dr(bx,by2,bw2,bh2,sc); \
          {char lbl[40]; snprintf(lbl,sizeof(lbl),"%s [%s]",(label),on?"ON":"OFF"); \
           dtc(bx+bw2/2,by2+bh2/2-FH,lbl,sc,2);} \
          if(hv&&g_mouse.clicked){action=(btn_id);g_mouse.clicked=0;} }
        #define CYC_ROW(label,val_label,sc,btn_id,row_y) \
        { int bx=px+12,by2=(row_y),bw2=pw-24,bh2=36; \
          int hv=pt_in(g_mouse.x,g_mouse.y,bx,by2,bw2,bh2); \
          fr(bx,by2,bw2,bh2,0x080820ff); \
          if(hv) fr(bx,by2,bw2,bh2,0xffffff18); \
          dr(bx,by2,bw2,bh2,(sc)); \
          {char lbl[48]; snprintf(lbl,sizeof(lbl),"%s [%s]",(label),(val_label)); \
           dtc(bx+bw2/2,by2+bh2/2-FH,lbl,(sc),2);} \
          if(hv&&g_mouse.clicked){action=(btn_id);g_mouse.clicked=0;} }
        OPT_ROW("Specials",  !c->no_attacks,      COL_OPEN_GREEN, 0x804040ff, BTN_TOGGLE_ATTACKS, py+44)
        OPT_ROW("Last Wins", c->last_player_wins,  COL_OPEN_GREEN, 0x804040ff, BTN_TOGGLE_ENDLESS, py+86)
        OPT_ROW("Taunts",   !c->no_taunts,        0xffaa00ff,     0x806040ff, BTN_TOGGLE_TAUNTS,  py+128)
        OPT_ROW("Speed Up",  c->speedup_enabled,   0xffcc00ff,     0x806040ff, BTN_TOGGLE_SPEEDUP, py+170)
        OPT_ROW("Hold Piece",c->hold_enabled,      COL_OPEN_GREEN, 0x804040ff, BTN_TOGGLE_HOLD,   py+212)
        {const char *gs_lbl=c->garbage_system?"Guideline":"Classic";
         CYC_ROW("Garbage",gs_lbl,0x6060c0ff,BTN_TOGGLE_GARBAGE_SYSTEM,py+254)}
        {const char *gt_lbl=c->garbage_targeting==0?"All":c->garbage_targeting==1?"Random":"Focused";
         CYC_ROW("Target",gt_lbl,0x8060c0ff,BTN_TOGGLE_GARBAGE_TARGET,py+296)}
        #undef OPT_ROW
        #undef CYC_ROW
        } else {
        /* ── Read-only status table for non-host players ── */
        #define STAT_ROW(label,val_str,val_col,row_y) \
        { int bx=px+12,by2=(row_y),bw2=pw-24,bh2=28; \
          fr(bx,by2,bw2,bh2,0x0a0a18ff); dr(bx,by2,bw2,bh2,0x222244ff); \
          dt(bx+10,by2+bh2/2-FH/2,(label),0x8080a0ff,1); \
          dt(bx+bw2-tw((val_str),1)-10,by2+bh2/2-FH/2,(val_str),(val_col),1); }
        STAT_ROW("Specials",  !c->no_attacks    ?"ON":"OFF", !c->no_attacks    ?COL_OPEN_GREEN:0x804040ff, py+44)
        STAT_ROW("Last Wins", c->last_player_wins?"ON":"OFF", c->last_player_wins?COL_OPEN_GREEN:0x804040ff, py+78)
        STAT_ROW("Taunts",   !c->no_taunts     ?"ON":"OFF", !c->no_taunts     ?0xffaa00ff:0x806040ff,     py+112)
        STAT_ROW("Speed Up",  c->speedup_enabled ?"ON":"OFF", c->speedup_enabled ?0xffcc00ff:0x806040ff,    py+146)
        STAT_ROW("Hold Piece",c->hold_enabled    ?"ON":"OFF", c->hold_enabled    ?COL_OPEN_GREEN:0x804040ff, py+180)
        {const char *gs=c->garbage_system?"Guideline":"Classic";
         STAT_ROW("Garbage",gs,0x8080ffff,py+214)}
        {const char *gt=c->garbage_targeting==0?"All":c->garbage_targeting==1?"Random":"Focused";
         STAT_ROW("Target",gt,0xa060c0ff,py+248)}
        #undef STAT_ROW
        dtc(px+pw/2,py+ph-52,"Only the host can change these settings.",0x505070ff,1);
        }
        if(dbtn(px+12,py+ph-44,pw-24,32,"Close",BS_NORMAL))
            c->game_options_popup_open=0;
        }
    }

    /* ── AI PLAYERS POPUP ── */
    if(c->ai_players_popup_open){
        int pw=320, ph=220;
        int px=(WIN_W-pw)/2, py=(WIN_H-ph)/2;
        int nbot=0, nplayers=1;
        for(int bi=0;bi<MAX_PLAYERS;bi++){
            if(c->opp_nicks[bi][0]) nplayers++;
            if(c->opp_is_bot[bi]) nbot++;}
        int room_full=(nplayers>=MAX_PLAYERS);
        SDL_SetRenderDrawBlendMode(g_rend,SDL_BLENDMODE_BLEND);
        fr(0,0,WIN_W,WIN_H,0x00000099);
        SDL_SetRenderDrawBlendMode(g_rend,SDL_BLENDMODE_NONE);
        g_mouse.clicked=saved_click_g;
        if(g_mouse.clicked&&!pt_in(g_mouse.x,g_mouse.y,px,py,pw,ph)){
            c->ai_players_popup_open=0; g_mouse.clicked=0; }
        else {
        g_popup_blocked=0;
        fr(px,py,pw,ph,0x10102aff); dr(px,py,pw,ph,0x6060c0ff);
        dtc(px+pw/2,py+12,"AI Players",COL_LTGRAY,2);
        {char bc[48]; snprintf(bc,sizeof(bc),"Bots: %d    Players: %d / %d",nbot,nplayers,MAX_PLAYERS);
         dtc(px+pw/2,py+32,bc,COL_GRAY,1);}
        {int bw3=(pw-24-4)/3;
         BtnStyle es=room_full?BS_DISABLED:BS_NORMAL;
         BtnStyle ms=room_full?BS_DISABLED:BS_NORMAL;
         BtnStyle hs=room_full?BS_DISABLED:BS_NORMAL;
         if(dbtn(px+12,          py+52,bw3,34,"Easy",  es)) action=BTN_ADD_BOT_EASY;
         if(dbtn(px+12+bw3+2,    py+52,bw3,34,"Medium",ms)) action=BTN_ADD_BOT_MEDIUM;
         if(dbtn(px+12+2*(bw3+2),py+52,bw3,34,"Hard",  hs)) action=BTN_ADD_BOT_HARD;}
        if(room_full) dtc(px+pw/2,py+93,"Room is full",0xff8080ff,1);
        {int half=(pw-24-4)/2;
         BtnStyle rs=nbot>0?BS_NORMAL:BS_DISABLED;
         if(dbtn(px+12,      py+102,half,34,"Remove Bot",rs)) action=BTN_REMOVE_BOT;
         if(dbtn(px+14+half, py+102,half,34,"Remove All",rs)){
             for(int bi=0;bi<MAX_PLAYERS;bi++) if(c->opp_is_bot[bi]){
                 char rbuf[32]; snprintf(rbuf,sizeof(rbuf),"removebot %d",bi+1);
                 net_send(c->sock,rbuf);}}}
        if(dbtn(px+12,py+ph-44,pw-24,32,"Close",BS_NORMAL))
            c->ai_players_popup_open=0;
        }
    }

    if(c->about_popup_open) render_about_popup(c,saved_click_g);
    if((c->state==CS_PLAYING || c->state==CS_GAMEOVER) && c->music_player_open) render_music_popup(c,saved_click_g);

    /* ── KEYMAP POPUP (game room) ── */
    if(c->keymap_popup_open) { render_keymap_popup(c, saved_click_g); }

    g_popup_blocked=0;
    if(action!=BTN_NONE) client_on_button(c,action);
    SDL_RenderPresent(g_rend);
}

/* SDL maps Xbox / PlayStation buttons to one portable layout. */
static void controller_open(void){
    if(g_controller && !SDL_GameControllerGetAttached(g_controller)){SDL_GameControllerClose(g_controller);g_controller=NULL;}
    if(!g_controller)for(int i=0;i<SDL_NumJoysticks();i++)if(SDL_IsGameController(i)){g_controller=SDL_GameControllerOpen(i);if(g_controller)break;}
}
static int controller_key(ClientCtx *c,int button){
    switch(button){
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:return c->km.move_left;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:return c->km.move_right;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:return c->km.soft_drop;
    case SDL_CONTROLLER_BUTTON_A:return c->km.rotate_cw;
    case SDL_CONTROLLER_BUTTON_B:return c->km.rotate_ccw;
    case SDL_CONTROLLER_BUTTON_X:case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:return c->km.hold;
    case SDL_CONTROLLER_BUTTON_Y:case SDL_CONTROLLER_BUTTON_DPAD_UP:return c->km.hard_drop;
    case SDL_CONTROLLER_BUTTON_START:return c->km.pause;
    default:return 0;
    }
}
static void controller_tick(ClientCtx *c){
    controller_open();Uint32 buttons=0,now=SDL_GetTicks();
    if(g_controller && c->controller_enabled && SDL_GetWindowFlags(g_win)&SDL_WINDOW_INPUT_FOCUS){
        for(int i=0;i<SDL_CONTROLLER_BUTTON_MAX;i++)if(SDL_GameControllerGetButton(g_controller,(SDL_GameControllerButton)i))buttons|=1u<<i;
        int x=SDL_GameControllerGetAxis(g_controller,SDL_CONTROLLER_AXIS_LEFTX),y=SDL_GameControllerGetAxis(g_controller,SDL_CONTROLLER_AXIS_LEFTY);
        if(x < -16000)buttons|=1u<<SDL_CONTROLLER_BUTTON_DPAD_LEFT;
        if(x > 16000)buttons|=1u<<SDL_CONTROLLER_BUTTON_DPAD_RIGHT;
        if(y > 16000)buttons|=1u<<SDL_CONTROLLER_BUTTON_DPAD_DOWN;
    }
    int blocked=c->keymap_popup_open||c->about_popup_open||c->music_player_open||c->game_options_popup_open||c->ai_players_popup_open||c->team_popup_open||c->kick_popup_open;
    int context=(int)c->state*2+blocked;
    if(context!=g_pad_context){g_pad_buttons=buttons;memset(g_pad_repeat,0,sizeof(g_pad_repeat));g_pad_context=context;return;}
    Uint32 pressed=buttons&~g_pad_buttons;g_pad_buttons=buttons;
    if(blocked)return;
    if(c->state==CS_PLAYING){
        for(int i=0;i<SDL_CONTROLLER_BUTTON_MAX;i++)if(pressed&(1u<<i)){int key=controller_key(c,i);if(key)client_on_key(c,key,0,0,NULL);}
        const int directions[]={SDL_CONTROLLER_BUTTON_DPAD_LEFT,SDL_CONTROLLER_BUTTON_DPAD_RIGHT,SDL_CONTROLLER_BUTTON_DPAD_DOWN};
        for(int i=0;i<3;i++){
            Uint32 bit=1u<<directions[i];
            if(!(buttons&bit))g_pad_repeat[i]=0;
            else if(pressed&bit)g_pad_repeat[i]=now+160;
            else if(g_pad_repeat[i] && (Sint32)(now-g_pad_repeat[i])>=0){client_on_key(c,controller_key(c,directions[i]),0,0,NULL);g_pad_repeat[i]=now+(i==2?45:65);}
        }
    }
}

/* ============================== SDL EVENTS ============================== */
int sdl_poll_events(ClientCtx *c) {
    SDL_Event ev;
    g_mouse.clicked = 0;
    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
        case SDL_QUIT: return 0;
        case SDL_KEYDOWN:
            client_on_key(c, ev.key.keysym.scancode, ev.key.keysym.sym, 0, NULL);
            break;
        case SDL_TEXTINPUT:
            client_on_key(c, 0, 0, 1, ev.text.text);
            break;
        case SDL_MOUSEWHEEL: {
            float amount=(float)ev.wheel.y;
#if SDL_VERSION_ATLEAST(2,0,18)
            if(ev.wheel.preciseY!=0) amount=ev.wheel.preciseY;
#endif
            if(ev.wheel.direction==SDL_MOUSEWHEEL_FLIPPED) amount=-amount;
            if(amount!=0) {
                int step=amount>0?1:-1;
                if(c->music_player_open) c->music_scroll-=step*2;
                else if(c->about_popup_open) c->credits_offset-=step*36;
            }
            break;
        }
        case SDL_MOUSEMOTION:
            g_mouse.x = ev.motion.x;
            g_mouse.y = ev.motion.y;
            break;
        case SDL_MOUSEBUTTONDOWN:
            if (ev.button.button == SDL_BUTTON_LEFT) {
                g_mouse.x = ev.button.x;
                g_mouse.y = ev.button.y;
                g_mouse.clicked = 1;
                g_mouse.held = 1;
            }
            break;
        case SDL_MOUSEBUTTONUP:
            if (ev.button.button == SDL_BUTTON_LEFT) g_mouse.held = 0;
            break;
        default: break;
        }
    }
    controller_tick(c);
    return c->running;
}

/* ============================== SDL INIT/SHUTDOWN ============================== */
void sdl_set_ui_options(int scale,int patterns,int contrast){
    g_patterns=patterns;g_high_contrast=contrast;
    if(scale!=75&&scale!=100&&scale!=125&&scale!=150)scale=100;
    if(g_win && scale!=g_ui_scale){
        SDL_Rect bounds={0,0,WIN_W*2,WIN_H*2};int display=SDL_GetWindowDisplayIndex(g_win);
        SDL_GetDisplayUsableBounds(display<0?0:display,&bounds);
        int w=WIN_W*scale/100,h=WIN_H*scale/100;
        if(w>bounds.w)w=bounds.w;if(h>bounds.h-40)h=bounds.h-40;
        SDL_SetWindowSize(g_win,w,h);
    }
    g_ui_scale=scale;
}
int sdl_init(void) {
    g_ui_scale=100;
    /* The game owns its entry point on every platform, including WinMain. */
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) < 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 0;
    }
    g_win = SDL_CreateWindow("LineClash",
                             SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                             WIN_W, WIN_H, SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE);
    if (!g_win) {
        fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        SDL_Quit();
        return 0;
    }
    g_rend = SDL_CreateRenderer(g_win, -1,
                                SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!g_rend) {
        g_rend = SDL_CreateRenderer(g_win, -1, SDL_RENDERER_SOFTWARE);
        if (!g_rend) {
            fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError());
            SDL_DestroyWindow(g_win); g_win = NULL;
            SDL_Quit();
            return 0;
        }
    }
    /* Fix HiDPI / Retina scaling: map our logical coordinate space onto
     * the full drawable framebuffer regardless of pixel density.
     * Without this, rendering only covers the top-left quarter on 2x displays. */
    SDL_RenderSetLogicalSize(g_rend, WIN_W, WIN_H);
    SDL_SetRenderDrawBlendMode(g_rend, SDL_BLENDMODE_NONE);
    ui_font_init();
    SDL_StartTextInput();
    return 1;
}

void sdl_shutdown(void) {
    if(g_controller){SDL_GameControllerClose(g_controller);g_controller=NULL;}
    g_pad_buttons=0;g_pad_context=-1;memset(g_pad_repeat,0,sizeof(g_pad_repeat));
    SDL_StopTextInput();
    if (g_ui_font) {SDL_DestroyTexture(g_ui_font);g_ui_font=NULL;}
    if (g_rend) { SDL_DestroyRenderer(g_rend); g_rend = NULL; }
    if (g_win)  { SDL_DestroyWindow(g_win);    g_win  = NULL; }
    SDL_Quit();
}

const char *sdl_get_res_dir(void) {
#ifdef __APPLE__
    /* When running as a macOS .app bundle, resources are in Contents/Resources/ */
    static char path[1024] = {0};
    if (path[0]) return path;
    char *base = SDL_GetBasePath();
    if (base) {
        snprintf(path, sizeof(path), "%s", base);
        SDL_free(base);
        /* If we're inside a bundle, base ends in Contents/MacOS/ — step up */
        char *p = strstr(path, "Contents/MacOS");
        if (p) { snprintf(p, sizeof(path)-(p-path), "Contents/Resources/"); }
    } else {
        snprintf(path, sizeof(path), "./");
    }
    return path;
#else
    static char path[1024] = {0};
    if (path[0]) return path;
    char *base = SDL_GetBasePath();
    if (base) { snprintf(path, sizeof(path), "%s", base); SDL_free(base); }
    else       { snprintf(path, sizeof(path), "./"); }
    return path;
#endif
}

void sdl_text_start(void) {
    SDL_StartTextInput();
}

void sdl_render_accounts(ClientCtx *c){
    SDL_SetRenderDrawColor(g_rend,0x0d,0x0d,0x1a,0xff);SDL_RenderClear(g_rend);
    fr(0,0,WIN_W,108,0x161d2dff);dt(48,28,"Your accounts",COL_WHITE,3);
    dt(48,80,"Choose an identity for public servers.",COL_LTGRAY,1);
    int busy=account_https_busy();
    dt(48,142,"Saved profiles",COL_LTGRAY,1);
    for(int i=0;i<c->account_count;i++){
        char label[64];snprintf(label,sizeof(label),"%s@%s%s",i==c->account_active?"Active: ":"",c->accounts[i].handle,c->accounts[i].token[0] && c->accounts[i].expires>time(NULL)?"  / signed in":"");
        if(dbtn(48,174+i*40,360,32,label,busy?BS_DISABLED:i==c->account_cursor?BS_ACTIVE:BS_NORMAL) && !busy)client_accounts_select(c,i);
    }
    if(!c->account_count)dt(48,182,"Add your first account on the right.",COL_GRAY,1);
    const int x=460,w=540;
    dt(x,142,"Sign in or add an account",COL_WHITE,2);
    dt(x,194,"Username",COL_LTGRAY,1);
    if(draw_textfield(x,218,w,32,&c->tf_account_handle,"Your central account") && !busy){c->tf_account_handle.focused=1;c->tf_account_password.focused=0;}
    dt(x,272,"Password",COL_LTGRAY,1);
    if(draw_passfield(x,296,w,32,&c->tf_account_password,"Password") && !busy){c->tf_account_handle.focused=0;c->tf_account_password.focused=1;}
    if(dbtn(x,354,w,36,busy?"Working securely...":"Sign in over HTTPS",busy?BS_DISABLED:BS_ACTIVE) && !busy)client_accounts_action(c,ACCOUNT_SIGNIN);
    dt(x,410,"Passwords and login tokens are never saved to disk.",COL_GRAY,1);
    dt(x,436,"Sign in once each time you open the client.",COL_GRAY,1);
    if(dbtn(x,484,w,32,"Create an account / recover password",BS_NORMAL))sdl_open_account_code("");
    if(dbtn(48,538,174,34,"Use account",busy?BS_DISABLED:BS_ACTIVE) && !busy)client_accounts_action(c,ACCOUNT_USE);
    if(dbtn(234,538,174,34,"Sign out",busy?BS_DISABLED:BS_NORMAL) && !busy)client_accounts_action(c,ACCOUNT_LOGOUT);
    if(dbtn(48,586,360,32,"Remove saved profile",busy?BS_DISABLED:BS_NORMAL) && !busy)client_accounts_action(c,ACCOUNT_REMOVE);
    char message[88];snprintf(message,sizeof(message),"%.87s",c->account_message);dt(48,666,message,COL_HOSTGOLD,1);
    if(dbtn(48,714,180,36,"Back",busy?BS_DISABLED:BS_NORMAL) && !busy)client_accounts_action(c,ACCOUNT_BACK);
    SDL_RenderPresent(g_rend);
}
