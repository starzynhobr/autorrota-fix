#define _CRT_SECURE_NO_WARNINGS
/*
 * autoroute_fix — fallback de origem para a Autorrota do cliente Grand Fantasia TW 750/751.
 *
 * Carregado como proxy de version.dll (o exe importa VERSION.dll): as exportacoes sao
 * encaminhadas para a DLL do sistema e, no DllMain, um jmp e instalado no ponto de decisao
 * apos o calculo de rota. Nenhum byte do exe em disco e alterado.
 *
 * Logica (mesma da build O4 v2 testada no GF Violet):
 *   1. rota original produzida ([ebx+0x70] > 0) -> fluxo normal de sucesso;
 *   2. senao, testa celulas na borda de aneis de raio 1..4, passo 0.5, em volta da origem;
 *      pula celulas bloqueadas e repete o calculo original com o MESMO destino;
 *   3. primeira rota nao vazia -> sucesso; esgotado -> restaura a origem e segue a falha original.
 *
 * Enderecos validos somente para o exe verificado em ExpectedBytes (TW 006.750.79.79).
 */
#include <windows.h>
#include <stdio.h>
#include "version_forward.h"

#define VA_DECISION     0x006D5931u  /* cmp [ebx+0x70],0 / mov [ebp-4],-1 / jg +0x3C   */
#define VA_ON_SUCCESS   0x006D597Au
#define VA_ON_FAILURE   0x006D593Eu
#define VA_CALC_ROUTE   0x006D9180u  /* thiscall(this=ebx+0x54, Vec2* dest, Vec2* origin, float) */
#define VA_IS_BLOCKED   0x00605C50u  /* thiscall(this=[0xDE1C28], float x, float y) -> al != 0 bloqueado */
#define VA_WORLD_PTR    0x00DE1C28u

static const BYTE ExpectedBytes[13] = {
    0x83, 0x7B, 0x70, 0x00,                   /* cmp dword ptr [ebx+0x70], 0 */
    0xC7, 0x45, 0xFC, 0xFF, 0xFF, 0xFF, 0xFF, /* mov dword ptr [ebp-4], -1   */
    0x7F, 0x3C                                /* jg 0x6D597A                 */
};

typedef struct { float x, y; } Vec2;
typedef void (__fastcall *CalcRouteFn)(void *self, void *edx, Vec2 *dest, Vec2 *origin, float param);
typedef BYTE (__fastcall *IsBlockedFn)(void *self, void *edx, float x, float y);

static FILE *g_log;
static volatile LONG g_retries, g_recovered, g_failed;

static void Log(const char *fmt, ...)
{
    va_list ap;
    if (!g_log) return;
    va_start(ap, fmt);
    vfprintf(g_log, fmt, ap);
    va_end(ap);
    fputc('\n', g_log);
    fflush(g_log);
}

static int RouteProduced(BYTE *ctx) { return *(int *)(ctx + 0x70) > 0; }

/* Chamado pelo stub com os registradores do frame original. Retorna 1 = sucesso, 0 = falha. */
static int __cdecl RetryWithNearbyOrigin(BYTE *ebp, BYTE *ebx)
{
    Vec2 *origin = (Vec2 *)(ebp - 0x1C);
    Vec2 *dest   = (Vec2 *)(ebp - 0x24);
    const Vec2 original = *origin;
    const float step = 0.5f;
    CalcRouteFn calc = (CalcRouteFn)VA_CALC_ROUTE;
    IsBlockedFn blocked = (IsBlockedFn)VA_IS_BLOCKED;
    void *world = *(void **)VA_WORLD_PTR;
    int radius, dx, dy;

    if (RouteProduced(ebx)) return 1;
    InterlockedIncrement(&g_retries);

    for (radius = 1; radius <= 4; ++radius) {
        for (dy = -radius; dy <= radius; ++dy) {
            for (dx = -radius; dx <= radius; ++dx) {
                Vec2 c;
                if (abs(dx) != radius && abs(dy) != radius) continue; /* so a borda do anel */
                c.x = original.x + dx * step;
                c.y = original.y + dy * step;
                if (world && blocked(world, NULL, c.x, c.y)) continue;
                *origin = c;
                calc(ebx + 0x54, NULL, dest, origin, *(float *)(ebx + 0xE0));
                if (RouteProduced(ebx)) {
                    InterlockedIncrement(&g_recovered);
                    Log("rota recuperada: origem (%.2f,%.2f) -> (%.2f,%.2f) raio %d destino (%.2f,%.2f)",
                        original.x, original.y, c.x, c.y, radius, dest->x, dest->y);
                    return 1;
                }
            }
        }
    }
    *origin = original;
    InterlockedIncrement(&g_failed);
    Log("sem rota apos fallback: origem (%.2f,%.2f) destino (%.2f,%.2f)",
        original.x, original.y, dest->x, dest->y);
    return 0;
}

/* Substitui os 13 bytes originais em VA_DECISION. */
static __declspec(naked) void DecisionStub(void)
{
    __asm {
        mov dword ptr [ebp-4], -1      ; instrucao original deslocada
        pushad
        push ebx
        push ebp
        call RetryWithNearbyOrigin
        add esp, 8
        mov [esp+28], eax              ; eax salvo pelo pushad
        popad
        test eax, eax
        jz failure
        push VA_ON_SUCCESS
        ret
    failure:
        push VA_ON_FAILURE
        ret
    }
}

static BOOL InstallHook(void)
{
    BYTE *at = (BYTE *)VA_DECISION;
    DWORD old;
    if (IsBadReadPtr(at, sizeof ExpectedBytes) || memcmp(at, ExpectedBytes, sizeof ExpectedBytes) != 0) {
        Log("exe nao reconhecido em 0x%08X; hook NAO instalado", VA_DECISION);
        return FALSE;
    }
    VirtualProtect(at, sizeof ExpectedBytes, PAGE_EXECUTE_READWRITE, &old);
    at[0] = 0xE9;
    *(DWORD *)(at + 1) = (DWORD)DecisionStub - (DWORD)at - 5;
    memset(at + 5, 0x90, sizeof ExpectedBytes - 5);
    VirtualProtect(at, sizeof ExpectedBytes, old, &old);
    FlushInstructionCache(GetCurrentProcess(), at, sizeof ExpectedBytes);
    Log("hook instalado em 0x%08X", VA_DECISION);
    return TRUE;
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        char path[MAX_PATH];
        DisableThreadLibraryCalls(inst);
        GetModuleFileNameA(inst, path, MAX_PATH);
        strcpy(strrchr(path, '\\') + 1, "autoroute_fix.log");
        g_log = fopen(path, "a");
        Log("--- autoroute_fix carregado");
        InstallHook();
    } else if (reason == DLL_PROCESS_DETACH) {
        Log("resumo: fallbacks %ld, recuperadas %ld, sem rota %ld", g_retries, g_recovered, g_failed);
        if (g_log) fclose(g_log);
    }
    return TRUE;
}
