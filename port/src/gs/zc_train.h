/* Treino de Z-Counter: o que port/src/zc_train.c guarda para o painel (gs/zc_train_ui.cpp) e as opções. */
#ifndef ZC_TRAIN_H
#define ZC_TRAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Resultado da última tentativa. */
enum {
    ZC_NONE = 0,
    ZC_PERFECT,  /* no frame certo: o counter saiu */
    ZC_EARLY,    /* diff frames antes do hit */
    ZC_LATE,     /* diff frames depois */
    ZC_MISSED,   /* o primeiro hit veio e não houve aperto */
    ZC_LOCKED,   /* apertou com a janela travada (um aperto anterior a menos de ~30 frames) */
    ZC_BADANIM   /* no frame certo, mas a sua animação naquele momento não permite counter */
};

/* Modos do boneco. */
enum {
    ZC_DUMMY_OFF = 0,
    ZC_DUMMY_SINGLE,  /* um golpe, ritmo fixo */
    ZC_DUMMY_COMBO,   /* o rush combo inteiro em loop */
    ZC_DUMMY_RANDOM,  /* um golpe, intervalo sorteado */
    ZC_DUMMY_STAND,   /* parado */
    ZC_DUMMY_GUARD,   /* só defende */
    ZC_DUMMY_COUNT
};

typedef struct {
    int seq;       /* muda a cada resultado novo */
    int kind;      /* ZC_* */
    int diff;      /* frames: negativo = cedo, positivo = tarde */
    int anim;      /* a animação permitia counter naquele hit */
    int flashSeq;  /* muda a cada primeiro hit de um golpe (o frame certo) */
    int flashAnim;
    int lockout;   /* frames de trava que faltam, 0 = livre */
    int tries, hits, streak, best, counters;
} PortZcView;

void PortZc_View(PortZcView *out);
void PortZc_ResetStats(void);

int PortZc_Overlay(void);
int PortZc_Dummy(void);
int PortZc_Period(void);
int PortZc_RefillOn(void);
int PortZc_DummyActive(void);
void PortZc_SetOverlay(int on);
void PortZc_SetDummy(int mode);
void PortZc_SetPeriod(int frames);
void PortZc_SetRefill(int on);

#ifdef __cplusplus
}
#endif

#endif
