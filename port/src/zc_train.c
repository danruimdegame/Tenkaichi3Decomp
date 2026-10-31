/*
 * Treino de Z-Counter (Cima + Quadrado): o avisador de timing e o boneco de treino.
 *
 * Como o jogo decide o counter (src/battle/btl_char_move.c BtlMove_UpdateDefenseTimers, btl_char_hit.c
 * BtlHit_CheckGuard): Cima e Quadrado recém-apertados no mesmo frame abrem a janela do lutador (+0x1070) por um
 * frame, e o counter sai quando o PRIMEIRO hit de um golpe do adversário é testado nesse mesmo frame, com a
 * animação do defensor tendo a flag 0x31. Apertar de novo antes de a janela voltar a -30 a trava outra vez
 * (cerca de 30 frames): apertar sem parar não funciona.
 *
 * Os ganchos (#ifdef PORT) em btl_char_move.c, btl_char_hit.c e btl_char_mgr.c contam aqui o que aconteceu em
 * cada frame: o aperto (aceito ou travado), o teste do primeiro hit de cada golpe e o counter que saiu. Isto
 * compara os frames e guarda o resultado para o painel (gs/zc_train_ui.cpp), que o desenha.
 *
 * O avisador só lê: não escreve na memória do jogo nem tira números do gerador do jogo. O boneco
 * (PortZc_DummyButtons, chamado em btl_ai_mgr.c) troca os botões da IA do lado da CPU por um roteiro, e a vida e
 * o ki infinitos (PortZc_Refill) escrevem nas barras: os dois só valem fora do online. As variáveis daqui não são
 * estado do jogo (port/tools/make_state.py).
 */
#include <stdint.h>
#include <string.h>

#include "gs/zc_train.h"

extern int gPortResim;              /* gs/state.c: frames sendo refeitos depois de um rollback */
extern int Port_NetActive(void);    /* gs/net.c */
extern int Port_Setting(const char *name, int def); /* plat_settings.c */
extern void Port_SettingSave(const char *name, int value);
extern void Port_SettingsWrite(void);

#define WAIT 15  /* frames antes / depois do hit em que um aperto ainda conta como tentativa */
#define FAR (-100000)

/* As opções (aba Treino do F1). */
static volatile int sLoaded, sOverlay, sDummy, sPeriod, sRefill;

static void load(void) {
    if (!sLoaded) {
        sOverlay = Port_Setting("zc_overlay", 1);
        sDummy = Port_Setting("zc_dummy", 0);
        sPeriod = Port_Setting("zc_period", 45);
        sRefill = Port_Setting("zc_refill", 1);
        if (sPeriod < 10 || sPeriod > 240) {
            sPeriod = 45;
        }
        sLoaded = 1;
    }
}

int PortZc_Overlay(void) { load(); return sOverlay; }
int PortZc_Dummy(void) { load(); return sDummy; }
int PortZc_Period(void) { load(); return sPeriod; }
int PortZc_RefillOn(void) { load(); return sRefill; }

void PortZc_SetOverlay(int on) { load(); sOverlay = on; Port_SettingSave("zc_overlay", on); Port_SettingsWrite(); }
void PortZc_SetDummy(int mode) { load(); sDummy = mode; Port_SettingSave("zc_dummy", mode); Port_SettingsWrite(); }
void PortZc_SetPeriod(int frames) { load(); sPeriod = frames; Port_SettingSave("zc_period", frames); }
void PortZc_SetRefill(int on) { load(); sRefill = on; Port_SettingSave("zc_refill", on); Port_SettingsWrite(); }

/* ---- O avisador ---- */

typedef struct {
    int prevWindow;   /* a janela (+0x1070) no fim do frame anterior */
    int pressFrame;   /* o último aperto ainda não usado, FAR se nenhum */
    int pressLocked;  /* ... feito com a janela travada */
    int pendFrame;    /* um primeiro hit à espera de um aperto atrasado, FAR se nenhum */
    int pendAnim;
    int lastHitFrame; /* o último resultado, para o counter do mesmo frame o corrigir */
} ZcSide;

static ZcSide sSide[2];
static int sFrame;        /* frames da luta (BtlChars_UpdateCollision) */
static int sCpu[2];       /* lado controlado pela CPU, visto neste frame */
static PortZcView sView;  /* o que o painel lê (outra thread: um valor rasgado só dura um quadro) */

static int human_side(void) {
    return sCpu[0] && !sCpu[1] ? 1 : 0;
}

static void post(int d, int kind, int diff, int anim) {
    if (d != human_side()) {
        return;
    }
    sView.kind = kind;
    sView.diff = diff;
    sView.anim = anim;
    if (kind != ZC_NONE && kind != ZC_MISSED) {
        sView.tries++;
        if (kind == ZC_PERFECT) {
            sView.hits++;
            sView.streak++;
            if (sView.streak > sView.best) {
                sView.best = sView.streak;
            }
        } else {
            sView.streak = 0;
        }
    }
    sView.seq++;
}

static void reset_side(ZcSide *s) {
    s->prevWindow = -30;
    s->pressFrame = FAR;
    s->pressLocked = 0;
    s->pendFrame = FAR;
    s->pendAnim = 0;
    s->lastHitFrame = FAR;
}

/* Fim da passagem de colisão: um frame da luta se passou. Chamado do jogo (btl_char_mgr.c). */
void PortZc_FrameEnd(void) {
    int d;

    if (gPortResim) {
        return;
    }
    for (d = 0; d < 2; d++) {
        ZcSide *s = &sSide[d];
        if (s->pendFrame != FAR && sFrame - s->pendFrame > WAIT) { /* o hit passou e ninguém apertou */
            post(d, ZC_MISSED, 0, s->pendAnim);
            s->pendFrame = FAR;
        }
        if (s->pressFrame != FAR && sFrame - s->pressFrame > WAIT) { /* apertou e nenhum golpe veio */
            s->pressFrame = FAR;
        }
    }
    sFrame++;
}

/* A janela do lutador `d` no fim de BtlMove_UpdateDefenseTimers. 1 = aperto aceito neste frame; -1 vindo de outro
   valor que não 0 = aperto feito com a janela travada (ela volta a -1 em vez de abrir). */
void PortZc_Window(int d, int window) {
    ZcSide *s;
    int press = 0, locked = 0;

    if (gPortResim || d < 0 || d > 1) {
        return;
    }
    s = &sSide[d];
    if (window == 1) {
        press = 1;
    } else if (window == -1 && s->prevWindow != 0) {
        press = 1;
        locked = 1;
    }
    s->prevWindow = window;
    if (d == human_side()) {
        sView.lockout = window > -30 && window <= 0 ? window + 30 : 0;
    }
    if (!press) {
        return;
    }
    if (s->pendFrame != FAR && sFrame - s->pendFrame <= WAIT) { /* atrasado: o hit já tinha vindo */
        post(d, locked ? ZC_LOCKED : ZC_LATE, sFrame - s->pendFrame, s->pendAnim);
        s->pendFrame = FAR;
        return;
    }
    s->pressFrame = sFrame;
    s->pressLocked = locked;
}

/* O golpe do adversário é testado contra o lutador `d` (BtlHit_TestHit). firstHit: é o primeiro hit do golpe, o
   único que pode ser counterado; anim: a animação de `d` tem a flag 0x31. */
void PortZc_HitTest(int d, int firstHit, int anim) {
    ZcSide *s;

    if (gPortResim || d < 0 || d > 1 || !firstHit) {
        return;
    }
    s = &sSide[d];
    if (d == human_side()) {
        sView.flashSeq++;
        sView.flashAnim = anim;
    }
    s->lastHitFrame = sFrame;
    if (s->pressFrame != FAR && sFrame - s->pressFrame <= WAIT) {
        int diff = s->pressFrame - sFrame;
        int kind;
        if (s->pressLocked) {
            kind = ZC_LOCKED;
        } else if (diff == 0) {
            kind = anim ? ZC_PERFECT : ZC_BADANIM; /* (PortZc_Counter confirma no mesmo frame) */
        } else {
            kind = ZC_EARLY;
        }
        post(d, kind, diff, anim);
        s->pressFrame = FAR;
        s->pendFrame = FAR;
        return;
    }
    s->pendFrame = sFrame;
    s->pendAnim = anim;
}

/* O counter saiu (BtlHit_CheckGuard, resultado 8) contra o golpe que mirava `d`. */
void PortZc_Counter(int d) {
    if (gPortResim || d < 0 || d > 1) {
        return;
    }
    if (d == human_side()) {
        sView.counters++;
        if (sView.kind != ZC_PERFECT || sSide[d].lastHitFrame != sFrame) {
            post(d, ZC_PERFECT, 0, 1);
        }
    }
}

void PortZc_SetCpu(int d, int cpu) {
    if (d >= 0 && d < 2) {
        sCpu[d] = cpu != 0;
    }
}

void PortZc_View(PortZcView *out) {
    *out = sView;
}

void PortZc_ResetStats(void) {
    sView.tries = sView.hits = sView.streak = sView.best = sView.counters = 0;
    sView.kind = ZC_NONE;
    sView.seq++;
}

/* ---- O boneco ---- */

typedef struct {
    int t;      /* frames desde o começo do ciclo */
    int next;   /* comprimento do ciclo atual (o aleatório sorteia um por ciclo) */
} ZcDummy;

static ZcDummy sDum[2];
static uint32_t sRand = 0x2545F491u; /* um gerador só daqui: o do jogo não é tocado */

static int rand_below(int n) {
    sRand = sRand * 1664525u + 1013904223u;
    return n > 0 ? (int)((sRand >> 8) % (uint32_t)n) : 0;
}

/* 1 = o boneco está ligado e pode valer (fora do online). */
int PortZc_DummyActive(void) {
    load();
    return sDummy != 0 && !Port_NetActive();
}

/* Os botões do boneco do lado `d` neste frame (bits BTLB_* de include/battle/btl_input.h), ou -1 para deixar a
   IA do jogo. close: o lutador está perto do adversário (flag 0x13). Chamado do jogo (btl_ai_mgr.c). */
int PortZc_DummyButtons(int d, int close) {
    enum { GUARD = 0x1, DASH = 0x2, RUSH = 0x8 };
    ZcDummy *m;
    int mode, period, t;

    if (!PortZc_DummyActive() || d < 0 || d > 1 || gPortResim) {
        return -1;
    }
    mode = sDummy;
    period = sPeriod;
    m = &sDum[d];
    if (mode == ZC_DUMMY_STAND) {
        m->t = 0;
        return 0;
    }
    if (!close) { /* longe: vai até o adversário (dash com a mira travada) e começa o ciclo ao chegar */
        m->t = 0;
        m->next = 0;
        return DASH;
    }
    if (m->next == 0) {
        m->next = mode == ZC_DUMMY_RANDOM ? period / 2 + rand_below(period + 1) : period;
    }
    t = m->t++;
    switch (mode) {
    case ZC_DUMMY_SINGLE:
    case ZC_DUMMY_RANDOM:
        /* um Quadrado (2 frames apertado), depois espera o resto do ciclo */
        if (t >= 6 + m->next) {
            m->t = 0;
            m->next = 0;
        }
        return t >= 6 && t < 8 ? RUSH : 0;
    case ZC_DUMMY_COMBO:
        /* o rush combo inteiro: 10 Quadrados, um a cada 5 frames; depois a pausa do ciclo */
        if (t < 50) {
            return t % 5 < 2 ? RUSH : 0;
        }
        if (t >= 50 + m->next) {
            m->t = 0;
            m->next = 0;
        }
        return 0;
    case ZC_DUMMY_GUARD:
        return GUARD; /* só defende: para treinar o seu próprio combo */
    }
    return -1;
}

/* O que repor neste frame: 1 vida, 2 ki (só com o boneco ligado, fora do online). */
int PortZc_Refill(void) {
    load();
    if (!sRefill || !PortZc_DummyActive() || gPortResim) {
        return 0;
    }
    return 3;
}

/* Começo de uma luta: tudo do zero menos os números da sessão. */
void PortZc_BattleStart(void) {
    reset_side(&sSide[0]);
    reset_side(&sSide[1]);
    memset(sDum, 0, sizeof(sDum));
    sView.kind = ZC_NONE;
    sView.lockout = 0;
    sView.seq++;
}
