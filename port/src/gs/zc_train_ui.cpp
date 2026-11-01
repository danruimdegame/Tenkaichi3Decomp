// Treino de Z-Counter: o painel sobre a luta e a aba "Treino" da janela do F1.
//
// O que aconteceu na luta vem de port/src/zc_train.c (escrito na thread do jogo, a cada frame); isto só desenha, na
// thread de renderização, na lista da frente do ImGui (chamado por ui.cpp).
//
// O painel: o resultado da última tentativa (PERFEITO / X frames cedo / tarde / travado), uma régua de -8 a +8
// frames com a marca de onde você apertou, uma luz que pisca no frame certo de cada golpe (verde quando a sua
// animação permitia o counter), a barra da trava e os números da sessão.
#include <math.h>
#include <stdio.h>
#include <SDL3/SDL.h>
#include "imgui.h"
#include "ui.h"
#include "zc_train.h"

extern "C" int Port_NetActive(void);        // gs/net.c
extern "C" int Port_NetSpectating(void);    // gs/net.c: esta cópia só assiste a uma partida
extern "C" void *BtlChar_FindByObjId(int objId); // o jogo: o lutador de um lado, NULL fora de uma luta

static int sSeenSeq = -1, sSeenFlash = -1;
static Uint64 sResultAt, sFlashAt;

// Se o painel deve aparecer agora.
bool ZcTrain_OverlayOn(void) {
    // (também no online: o painel só lê o jogo; não para quem só assiste, que não tem lado)
    return PortZc_Overlay() && !Port_NetSpectating() && BtlChar_FindByObjId(0) != NULL && gUiPresentW > 0 && gUiPresentH > 0;
}

static void outlined(ImDrawList *dl, ImFont *font, float size, ImVec2 p, ImU32 col, const char *text) {
    float o = size * 0.06f + 1.0f;
    for (int ox = -1; ox <= 1; ox++) {
        for (int oy = -1; oy <= 1; oy++) {
            if (ox != 0 || oy != 0) {
                dl->AddText(font, size, ImVec2(p.x + (float)ox * o, p.y + (float)oy * o), IM_COL32(0, 0, 0, (col >> 24) & 0xFF), text);
            }
        }
    }
    dl->AddText(font, size, p, col, text);
}

static ImU32 with_alpha(ImU32 col, float a) {
    int alpha = (int)((float)((col >> 24) & 0xFF) * (a < 0.0f ? 0.0f : a > 1.0f ? 1.0f : a));
    return (col & 0x00FFFFFFu) | ((ImU32)alpha << 24);
}

void ZcTrain_Draw(void) {
    PortZcView v;
    PortZc_View(&v);
    Uint64 now = SDL_GetTicks();
    if (v.seq != sSeenSeq) {
        sSeenSeq = v.seq;
        sResultAt = now;
    }
    if (v.flashSeq != sSeenFlash) {
        sSeenFlash = v.flashSeq;
        sFlashAt = now;
    }

    ImDrawList *dl = ImGui::GetForegroundDrawList();
    ImFont *font = ImGui::GetFont();
    float W = (float)gUiPresentW, H = (float)gUiPresentH;
    float s = H / 448.0f;                     // em pixels do jogo (448 de altura)
    float pw = 230.0f * s, ph = 74.0f * s;
    float x0 = (float)gUiPresentX + (W - pw) * 0.5f, y0 = (float)gUiPresentY + H - ph - 40.0f * s;
    float big = 17.0f * s, small = 9.5f * s;

    dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x0 + pw, y0 + ph), IM_COL32(8, 12, 30, 170), 6.0f * s);

    // A luz do frame certo: acende por 150 ms a cada primeiro hit de um golpe.
    float lx = x0 + 14.0f * s, ly = y0 + 16.0f * s, lr = 7.0f * s;
    float lit = 1.0f - (float)(now - sFlashAt) / 150.0f;
    ImU32 lampOn = v.flashAnim ? IM_COL32(80, 255, 120, 255) : IM_COL32(170, 170, 170, 255);
    dl->AddCircleFilled(ImVec2(lx, ly), lr, lit > 0.0f && sSeenFlash > 0 ? with_alpha(lampOn, 0.4f + 0.6f * lit) : IM_COL32(40, 50, 60, 220));
    dl->AddCircle(ImVec2(lx, ly), lr, IM_COL32(255, 255, 255, 140), 0, 1.0f + s * 0.5f);

    // O resultado.
    char text[64];
    ImU32 col = IM_COL32(255, 255, 255, 255);
    switch (v.kind) {
    case ZC_PERFECT: snprintf(text, sizeof(text), "PERFEITO!"); col = IM_COL32(90, 255, 130, 255); break;
    case ZC_EARLY:
        snprintf(text, sizeof(text), "%d frame%s CEDO", -v.diff, v.diff == -1 ? "" : "s");
        col = IM_COL32(255, 220, 80, 255);
        break;
    case ZC_LATE:
        snprintf(text, sizeof(text), "%d frame%s TARDE", v.diff, v.diff == 1 ? "" : "s");
        col = IM_COL32(255, 150, 60, 255);
        break;
    case ZC_MISSED: snprintf(text, sizeof(text), "sem input"); col = IM_COL32(150, 160, 175, 255); break;
    case ZC_LOCKED: snprintf(text, sizeof(text), "TRAVADO (apertou de novo)"); col = IM_COL32(255, 80, 80, 255); break;
    case ZC_BADANIM: snprintf(text, sizeof(text), "na hora, mas sem counter aqui"); col = IM_COL32(190, 190, 255, 255); break;
    default: snprintf(text, sizeof(text), "Z-Counter: Cima + Quadrado"); col = IM_COL32(200, 210, 230, 255); break;
    }
    float age = (float)(now - sResultAt) / 1000.0f;
    float fade = v.kind == ZC_NONE ? 0.8f : age < 1.5f ? 1.0f : 1.0f - (age - 1.5f) / 1.0f;
    float tsize = v.kind == ZC_NONE || v.kind == ZC_BADANIM || v.kind == ZC_LOCKED ? big * 0.75f : big;
    ImVec2 ext = font->CalcTextSizeA(tsize, 10000.0f, 0.0f, text);
    outlined(dl, font, tsize, ImVec2(x0 + (pw - ext.x) * 0.5f + 8.0f * s, ly - ext.y * 0.5f), with_alpha(col, fade < 0.25f ? 0.25f : fade), text);

    // A régua: -8 .. +8 frames, o meio é o frame certo.
    const int N = 8;
    float rx0 = x0 + 12.0f * s, rx1 = x0 + pw - 12.0f * s, ry = y0 + 36.0f * s, rh = 12.0f * s;
    float cw = (rx1 - rx0) / (float)(2 * N + 1);
    for (int i = -N; i <= N; i++) {
        float cx = rx0 + (float)(i + N) * cw;
        ImU32 c = i == 0 ? IM_COL32(60, 140, 80, 230) : IM_COL32(50, 60, 80, 200);
        dl->AddRectFilled(ImVec2(cx + 1.0f, ry), ImVec2(cx + cw - 1.0f, ry + rh), c, 2.0f * s);
    }
    if (v.kind == ZC_PERFECT || v.kind == ZC_EARLY || v.kind == ZC_LATE || v.kind == ZC_BADANIM || v.kind == ZC_LOCKED) {
        int d = v.diff < -N ? -N : v.diff > N ? N : v.diff;
        float cx = rx0 + (float)(d + N) * cw;
        dl->AddRectFilled(ImVec2(cx + 1.0f, ry - 2.0f * s), ImVec2(cx + cw - 1.0f, ry + rh + 2.0f * s), with_alpha(col, fade < 0.35f ? 0.35f : fade),
                          2.0f * s);
    }
    outlined(dl, font, small, ImVec2(rx0, ry + rh + 1.0f * s), IM_COL32(170, 180, 200, 230), "cedo");
    ImVec2 te = font->CalcTextSizeA(small, 10000.0f, 0.0f, "tarde");
    outlined(dl, font, small, ImVec2(rx1 - te.x, ry + rh + 1.0f * s), IM_COL32(170, 180, 200, 230), "tarde");

    // A trava: some quando você pode tentar de novo.
    if (v.lockout > 0) {
        float frac = (float)v.lockout / 30.0f;
        dl->AddRectFilled(ImVec2(rx0, ry - 5.0f * s), ImVec2(rx0 + (rx1 - rx0) * frac, ry - 3.0f * s), IM_COL32(255, 80, 80, 220));
    }

    // Os números da sessão.
    snprintf(text, sizeof(text), "acertos %d/%d   sequencia %d   melhor %d", v.hits, v.tries, v.streak, v.best);
    te = font->CalcTextSizeA(small, 10000.0f, 0.0f, text);
    outlined(dl, font, small, ImVec2(x0 + (pw - te.x) * 0.5f, y0 + ph - small - 4.0f * s), IM_COL32(220, 225, 240, 230), text);
}

// A aba "Treino" da janela do F1.
void ZcTrain_Tab(float scale) {
    bool overlay = PortZc_Overlay() != 0;
    if (ImGui::Checkbox("Avisador de Z-Counter na tela", &overlay)) {
        PortZc_SetOverlay(overlay ? 1 : 0);
    }
    ImGui::SetItemTooltip("Mostra, a cada golpe do adversario, se o seu Cima + Quadrado veio cedo, tarde ou no frame certo.\n"
                          "A luz pisca no frame exato do primeiro hit de cada golpe (verde: ali o counter era possivel).\n"
                          "So le o jogo: nao muda a luta, e por isso tambem aparece no online.");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    static const char *const kModes[ZC_DUMMY_COUNT] = {"Desligado (IA normal)", "Golpe unico, ritmo fixo", "Rush combo em loop",
                                                       "Golpe unico, intervalo aleatorio", "Parado", "So defende"};
    int mode = PortZc_Dummy();
    if (mode < 0 || mode >= ZC_DUMMY_COUNT) {
        mode = 0;
    }
    ImGui::SetNextItemWidth(260.0f * scale);
    if (ImGui::Combo("Boneco de treino (CPU)", &mode, kModes, ZC_DUMMY_COUNT)) {
        PortZc_SetDummy(mode);
    }
    ImGui::SetItemTooltip("Troca a IA do lutador controlado pela CPU por um roteiro. Use numa luta contra a CPU\n"
                          "(por exemplo Duelo, Jogador x CPU). Nao vale no online.");
    int period = PortZc_Period();
    ImGui::SetNextItemWidth(260.0f * scale);
    if (ImGui::SliderInt("Intervalo (frames)", &period, 10, 180, "%d", ImGuiSliderFlags_AlwaysClamp)) {
        PortZc_SetPeriod(period);
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        PortZc_SetDummy(PortZc_Dummy()); // (grava o arquivo de opcoes)
    }
    ImGui::SetItemTooltip("Espera entre um golpe e o proximo (no combo: entre um combo e o proximo).\n"
                          "30 frames = 1 segundo.");
    bool refill = PortZc_RefillOn() != 0;
    if (ImGui::Checkbox("Vida e ki infinitos (com o boneco ligado)", &refill)) {
        PortZc_SetRefill(refill ? 1 : 0);
    }
    ImGui::Spacing();
    if (ImGui::Button("Zerar os numeros")) {
        PortZc_ResetStats();
    }
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextWrapped("Como o jogo decide: Cima e Quadrado precisam ser apertados juntos, no mesmo frame, exatamente no frame "
                       "do primeiro hit do golpe do adversario (uma janela de 1 frame, 1/30 s). Apertar de novo antes de "
                       "passar cerca de 1 segundo trava a janela: apertar sem parar nao funciona.");
    if (PortZc_Dummy() != 0 && Port_NetActive()) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.3f, 1.0f), "O boneco fica desligado durante o online.");
    }
}
