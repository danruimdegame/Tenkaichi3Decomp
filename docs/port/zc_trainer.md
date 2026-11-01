# Treino de Z-Counter

Um mod de treino para o Z-Counter (Cima + Quadrado). Fica na janela do F1, aba **Treino**.

## Como o jogo decide o counter

- `src/battle/btl_input.c`, condição de input 38: Cima e Quadrado recém-apertados **no mesmo frame**.
- `src/battle/btl_char_move.c`, `BtlMove_UpdateDefenseTimers`: a janela do lutador (+0x1070) fica em -30; o
  aperto a põe em 1. Ela desce um por frame. Apertar de novo antes de ela voltar a -30 a põe em -1: uma trava de
  cerca de 30 frames.
- `src/battle/btl_char_hit.c`, `BtlHit_CheckGuard`: o counter (resultado de guarda 8) sai quando é o **primeiro
  hit** do golpe do atacante, a animação do defensor tem a flag 0x31 e a janela está acima de 0.
- Os timers rodam antes da colisão no mesmo frame, então a janela só vale no frame do próprio aperto: **1 frame**
  (1/30 s), o frame em que o primeiro hit é testado.

## O que o mod faz

- **Avisador** (painel embaixo, no meio da luta): o resultado de cada tentativa (PERFEITO, X frames cedo, X frames
  tarde, travado, ou "na hora, mas sem counter aqui" quando a sua animação naquele momento não tem a flag 0x31),
  uma régua de -8 a +8 frames, uma luz que pisca no frame exato do primeiro hit de cada golpe (verde quando o
  counter era possível), a barra da trava e os números da sessão. Só lê o jogo, então também aparece no online
  (para quem joga, não para quem assiste).
- **Boneco de treino**: troca os botões da IA do lado da CPU por um roteiro: golpe único em ritmo fixo, rush combo
  em loop, golpe único com intervalo aleatório, parado, ou só defende. Longe do adversário ele vai até você (dash).
- **Vida e ki infinitos** enquanto o boneco está ligado.

O boneco e a vida infinita escrevem no estado da luta: ficam desligados no online.

## Onde está

| Arquivo | O quê |
|---|---|
| `port/src/zc_train.c` | a lógica: compara os frames, guarda o resultado, o roteiro do boneco, as opções |
| `port/src/gs/zc_train.h` | o que o painel lê |
| `port/src/gs/zc_train_ui.cpp` | o painel e a aba Treino |
| `src/battle/btl_char_move.c`, `btl_char_hit.c`, `btl_char_mgr.c` | ganchos `#ifdef PORT` que só contam o que aconteceu |
| `src/battle/btl_ai_mgr.c` | gancho `#ifdef PORT`: os botões do boneco e a reposição de vida e ki |

Opções gravadas: `zc_overlay`, `zc_dummy`, `zc_period`, `zc_refill`.

## Versão sempre atualizada

`.github/workflows/treino.yml` roda todo dia: se o projeto oficial (z3xox/Tenkaichi3Decomp) tem coisa nova, junta
com o mod, compila o zip do Windows e o publica como release. O download mais recente fica em
`https://github.com/danruimdegame/Tenkaichi3Decomp/releases/latest`, sem precisar de conta no GitHub.
