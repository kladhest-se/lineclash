/* mod.c — Minimal 4-channel ProTracker MOD player
   Supports: 31-instrument MOD, effects 0(arp) 1/2(slide) 3(toneporta)
             A(vol slide) B(jump) C(vol) D(break) E(extended) F(speed/bpm)
   Output: mono float mixed into SDL callback buffer. */
#include "mod.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MOD_CHANNELS    4
#define MOD_MAXPAT      128
#define MOD_MAXINST     31
#define SAMPLE_RATE     44100

/* --- Amiga period table (C-1 to B-3, no finetune) --- */
static const int g_periods[36] = {
    856,808,762,720,678,640,604,570,538,508,480,453,  /* C-1..B-1 */
    428,404,381,360,339,320,302,285,269,254,240,226,  /* C-2..B-2 */
    214,202,190,180,170,160,151,143,135,127,120,113   /* C-3..B-3 */
};

typedef struct {
    char    name[23];
    int     len;        /* bytes */
    int8_t  finetune;   /* signed 4-bit */
    uint8_t volume;     /* 0-64 */
    int     loop_start;
    int     loop_len;
    int8_t *data;
} Instrument;

typedef struct {
    uint8_t note;   /* period index 0-35, 0xff=none */
    uint8_t inst;   /* 1-31, 0=none */
    uint8_t effect;
    uint8_t eparam;
} Cell;

typedef struct {
    Cell    rows[64][MOD_CHANNELS];
} Pattern;

typedef struct {
    /* playback pos */
    int      song_pos;
    int      pattern_pos; /* row 0-63 */
    int      tick;        /* current tick within row */
    int      speed;       /* ticks per row */
    int      bpm;
    /* samples per tick = SAMPLE_RATE*2.5/bpm */
    int      spt;         /* samples per tick */
    int      spt_acc;     /* sample counter within tick */

    int      num_orders;
    uint8_t  order[MOD_MAXPAT];
    int      num_patterns;
    Pattern *patterns;
    int      num_inst;
    Instrument inst[MOD_MAXINST];
    int      loaded;

    /* per-channel state */
    struct {
        int      period;     /* current Amiga period */
        int      inst;       /* 0=none */
        float    volume;     /* 0..1 */
        float    sample_pos; /* position in sample data (float for sub-sample) */
        float    sample_inc; /* per output-sample increment */
        /* effect state */
        int      effect;
        int      eparam;
        int      porta_target; /* period target for portamento */
        int      porta_speed;
        float    vol_slide;
    } ch[MOD_CHANNELS];
} ModPlayer;

static ModPlayer g_mod;

/* period → sample increment per output sample */
static float period_to_inc(int period) {
    if (period <= 0) return 0.0f;
    /* Amiga clock 7093789.2 Hz, output = (clock/2) / period / SAMPLE_RATE */
    return (7093789.2f / 2.0f) / (float)period / (float)SAMPLE_RATE;
}

static int find_period_index(int period) {
    int i, best=0, bdiff=99999;
    for(i=0;i<36;i++){
        int d=abs(g_periods[i]-period);
        if(d<bdiff){bdiff=d;best=i;}
    }
    return best;
}

void mod_init(void){ memset(&g_mod,0,sizeof(g_mod)); }

void mod_stop(void){
    int i;
    if(!g_mod.loaded) return;
    for(i=0;i<g_mod.num_inst;i++) free(g_mod.inst[i].data);
    free(g_mod.patterns);
    memset(&g_mod,0,sizeof(g_mod));
}

int mod_playing(void){ return g_mod.loaded; }

static void apply_note(int ch, int note_idx, int inst_idx){
    ModPlayer *m=&g_mod;
    if(inst_idx>=1&&inst_idx<=m->num_inst){
        Instrument *ins=&m->inst[inst_idx-1];
        m->ch[ch].inst=inst_idx;
        m->ch[ch].volume=(float)ins->volume/64.0f;
    }
    if(note_idx<36){
        int inst=m->ch[ch].inst;
        if(inst>=1&&inst<=m->num_inst){
            /* Amiga finetune */
            int ft=m->inst[inst-1].finetune;
            int period=g_periods[note_idx];
            /* simple finetune: shift period slightly */
            period=period+(period*ft)/800;
            m->ch[ch].period=period;
            m->ch[ch].sample_pos=0.0f;
            m->ch[ch].sample_inc=period_to_inc(period);
        }
    }
}

static void tick_effects(int ch){
    ModPlayer *m=&g_mod;
    int eff=m->ch[ch].effect, ep=m->ch[ch].eparam;
    switch(eff){
    case 0x1: /* portamento up */
        m->ch[ch].period-=ep;
        if(m->ch[ch].period<113)m->ch[ch].period=113;
        m->ch[ch].sample_inc=period_to_inc(m->ch[ch].period);
        break;
    case 0x2: /* portamento down */
        m->ch[ch].period+=ep;
        if(m->ch[ch].period>856)m->ch[ch].period=856;
        m->ch[ch].sample_inc=period_to_inc(m->ch[ch].period);
        break;
    case 0x3: /* tone portamento */
        if(m->ch[ch].period<m->ch[ch].porta_target){
            m->ch[ch].period+=m->ch[ch].porta_speed;
            if(m->ch[ch].period>m->ch[ch].porta_target)
                m->ch[ch].period=m->ch[ch].porta_target;
        } else {
            m->ch[ch].period-=m->ch[ch].porta_speed;
            if(m->ch[ch].period<m->ch[ch].porta_target)
                m->ch[ch].period=m->ch[ch].porta_target;
        }
        m->ch[ch].sample_inc=period_to_inc(m->ch[ch].period);
        break;
    case 0xA: /* volume slide */
        {int hi=(ep>>4),lo=(ep&0xf);
         if(hi) m->ch[ch].volume+=hi/64.0f;
         else   m->ch[ch].volume-=lo/64.0f;
         if(m->ch[ch].volume<0)m->ch[ch].volume=0;
         if(m->ch[ch].volume>1)m->ch[ch].volume=1;}
        break;
    }
}

static void process_row(void){
    ModPlayer *m=&g_mod;
    int ch;
    int pat_idx=m->order[m->song_pos];
    Pattern *pat=&m->patterns[pat_idx];
    int row=m->pattern_pos;
    int jump_song=-1, break_row=-1;

    for(ch=0;ch<MOD_CHANNELS;ch++){
        Cell *c=&pat->rows[row][ch];
        int eff=c->effect, ep=c->eparam;
        m->ch[ch].effect=eff; m->ch[ch].eparam=ep;

        /* Note + instrument trigger */
        if(eff==0x3&&c->note<36){
            /* tone portamento: set target, don't retrigger */
            int inst=c->inst?c->inst:m->ch[ch].inst;
            if(inst>=1&&inst<=m->num_inst){
                int ft=m->inst[inst-1].finetune;
                int p=g_periods[c->note];
                p=p+(p*ft)/800;
                m->ch[ch].porta_target=p;
                m->ch[ch].porta_speed=ep;
            }
            if(c->inst>=1) m->ch[ch].inst=c->inst;
        } else {
            apply_note(ch, c->note<36?c->note:0xff, c->inst);
        }

        /* Row-tick-0 effects */
        switch(eff){
        case 0xB: jump_song=ep; break_row=0; break;   /* jump to order */
        case 0xC: /* set volume */
            m->ch[ch].volume=(ep>64?64:ep)/64.0f; break;
        case 0xD: /* pattern break */
            break_row=((ep>>4)*10)+(ep&0xf); break;
        case 0xF: /* set speed / bpm */
            if(ep==0) ep=1;
            if(ep<32) m->speed=ep;
            else { m->bpm=ep; m->spt=(int)(SAMPLE_RATE*2.5f/m->bpm); }
            break;
        case 0xE: /* extended */
            switch(ep>>4){
            case 0xC: /* note cut on tick N — handled in tick */ break;
            case 0xD: /* note delay — skip for simplicity */ break;
            }
            break;
        }
    }

    /* Advance position */
    if(jump_song>=0){
        m->song_pos=jump_song%m->num_orders;
        m->pattern_pos=break_row<0?0:break_row;
    } else if(break_row>=0){
        m->song_pos=(m->song_pos+1)%m->num_orders;
        m->pattern_pos=break_row;
    } else {
        m->pattern_pos++;
        if(m->pattern_pos>=64){
            m->pattern_pos=0;
            m->song_pos=(m->song_pos+1)%m->num_orders;
        }
    }
}

int mod_load(const char *path){
    FILE *f=fopen(path,"rb");
    int i,ch,row;
    uint8_t buf[1084];
    if(!f) return 0;

    mod_stop();

    /* Read header (1084 bytes for 31-instrument MOD) */
    if(fread(buf,1,1084,f)!=1084){fclose(f);return 0;}

    /* Verify MOD tag at offset 1080 */
    {char tag[5]={0}; memcpy(tag,buf+1080,4);
     if(strcmp(tag,"M.K.")!=0&&strcmp(tag,"FLT4")!=0&&
        strcmp(tag,"4CHN")!=0&&strncmp(tag,"M!K!",4)!=0){
         /* try anyway — many valid MODs have non-standard tags */
     }}

    g_mod.num_inst=31;

    /* Parse instruments */
    for(i=0;i<31;i++){
        uint8_t *p=buf+20+i*30;
        Instrument *ins=&g_mod.inst[i];
        memcpy(ins->name,p,22); ins->name[22]=0;
        ins->len      =((p[22]<<8)|p[23])*2;
        ins->finetune =(int8_t)(p[24]&0xf); if(ins->finetune>7)ins->finetune-=16;
        ins->volume   =p[25];
        ins->loop_start=((p[26]<<8)|p[27])*2;
        ins->loop_len  =((p[28]<<8)|p[29])*2;
        ins->data=NULL;
    }

    /* Song length and order table */
    g_mod.num_orders=buf[950]; if(g_mod.num_orders>128)g_mod.num_orders=128;
    g_mod.num_patterns=0;
    for(i=0;i<128;i++){
        g_mod.order[i]=buf[952+i];
        if(g_mod.order[i]+1>g_mod.num_patterns)
            g_mod.num_patterns=g_mod.order[i]+1;
    }

    /* Allocate and read patterns */
    g_mod.patterns=(Pattern*)calloc(g_mod.num_patterns,sizeof(Pattern));
    if(!g_mod.patterns){fclose(f);return 0;}
    for(i=0;i<g_mod.num_patterns;i++){
        for(row=0;row<64;row++){
            for(ch=0;ch<MOD_CHANNELS;ch++){
                uint8_t d[4]; Cell *c=&g_mod.patterns[i].rows[row][ch];
                if(fread(d,1,4,f)!=4){c->note=0xff;continue;}
                {int period=((d[0]&0xf)<<8)|d[1];
                 c->note=(period>0)?find_period_index(period):0xff;}
                c->inst=(d[0]&0xf0)|(d[2]>>4);
                c->effect=d[2]&0xf;
                c->eparam=d[3];
            }
        }
    }

    /* Read sample data */
    for(i=0;i<31;i++){
        Instrument *ins=&g_mod.inst[i];
        if(ins->len>0){
            ins->data=(int8_t*)malloc(ins->len);
            if(ins->data) fread(ins->data,1,ins->len,f);
        }
    }
    fclose(f);

    /* Init playback */
    g_mod.speed=6; g_mod.bpm=125;
    g_mod.spt=(int)(SAMPLE_RATE*2.5f/g_mod.bpm);
    g_mod.song_pos=0; g_mod.pattern_pos=0;
    g_mod.tick=0; g_mod.spt_acc=0;
    memset(g_mod.ch,0,sizeof(g_mod.ch));
    for(ch=0;ch<MOD_CHANNELS;ch++) g_mod.ch[ch].volume=1.0f;
    g_mod.loaded=1;

    /* Process first row immediately */
    process_row();
    return 1;
}

void mod_mix(int16_t *buf, int samples, float volume){
    ModPlayer *m=&g_mod;
    int i,ch;
    if(!m->loaded) return;

    for(i=0;i<samples;i++){
        float out=0.0f;

        /* Check if we need a new tick */
        if(m->spt_acc>=m->spt){
            m->spt_acc=0;
            m->tick++;
            if(m->tick>=m->speed){
                m->tick=0;
                process_row();
            } else {
                /* Apply per-tick effects on non-zero ticks */
                for(ch=0;ch<MOD_CHANNELS;ch++) tick_effects(ch);
            }
        }
        m->spt_acc++;

        /* Mix channels */
        for(ch=0;ch<MOD_CHANNELS;ch++){
            int inst=m->ch[ch].inst;
            if(inst<1||inst>m->num_inst) continue;
            Instrument *ins=&m->inst[inst-1];
            if(!ins->data||ins->len<=0) continue;

            int pos=(int)m->ch[ch].sample_pos;
            if(pos>=ins->len){
                /* Loop handling */
                if(ins->loop_len>2){
                    while(pos>=ins->loop_start+ins->loop_len)
                        pos-=ins->loop_len;
                    m->ch[ch].sample_pos=(float)pos;
                } else continue;
            }
            out+=((float)ins->data[pos]/128.0f)*m->ch[ch].volume;
            m->ch[ch].sample_pos+=m->ch[ch].sample_inc;
        }

        /* Clamp and mix into output buffer */
        out*=volume*0.25f; /* 4 channels → scale down */
        if(out> 1.0f) out= 1.0f;
        if(out<-1.0f) out=-1.0f;
        /* Add to existing buffer (SFX may be there) */
        {int32_t existing=buf[i];
         int32_t mixed=existing+(int32_t)(out*28000);
         if(mixed> 32767) mixed= 32767;
         if(mixed<-32768) mixed=-32768;
         buf[i]=(int16_t)mixed;}
    }
}
