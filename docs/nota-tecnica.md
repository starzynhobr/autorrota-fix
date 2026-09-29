# Autorrota: fallback de origem no cliente — nota técnica

Atualização da nota publicada no GF pServer. A primeira versão foi um patch direto no executável do GF Violet, validado só no servidor do Violet. Esta versão:

- implementa o mesmo fallback como **DLL carregada pelo cliente**, sem alterar o exe em disco;
- foi testada num **servidor 751 local** (pacote TW 751 + dados “Budda Edition”) com o cliente TW `006.750.79.79`;
- registra em log cada rota recuperada, o que revelou um padrão nas falhas.

## Sintoma

A Autorrota exibe **“Não pode ser movido para o lugar de uso”** (`T_TextIndex` 11077 nos clientes em português) em trajetos alcançáveis, com frequência no Palácio de Proteção do Selo (mapa 260). Repetir o comando às vezes funciona; não há trajeto que falhe sempre.

## Fluxo no cliente

Endereços do exe TW `006.750.79.79` (SHA-256 `B43E2AF3…40EFBB1`, 32 bits). O executável do GF Violet é este mesmo exe com recursos e um carregador de DLL alterados; os endereços abaixo coincidem.

| Endereço | Papel |
|---|---|
| `0x006D592C` | chamada ao cálculo de rota |
| `0x006D9180` | cálculo de rota — `thiscall(this = ebx+0x54, Vec2* destino, Vec2* origem, float)`, origem em `ebp-0x1C`, destino em `ebp-0x24`, float em `[ebx+0xE0]` |
| `0x006D5931` | decisão: `cmp [ebx+0x70],0` / `mov [ebp-4],-1` / `jg 0x006D597A` |
| `0x006D597A` | fluxo de rota produzida |
| `0x006D593E` | fluxo de falha (mensagem 11077) |
| `0x00605C50` | validação de posição — `thiscall(this = [0x00DE1C28], float x, float y)`, `al != 0` = bloqueada |

Bytes originais em `0x006D5931`: `83 7B 70 00 C7 45 FC FF FF FF FF 7F 3C`.

## Correção

Um `jmp` em `0x006D5931` leva a um stub que executa a instrução deslocada (`mov [ebp-4],-1`) e chama o fallback com `ebp`/`ebx` do frame original:

1. `[ebx+0x70] > 0` → segue para `0x006D597A` (nada muda quando a rota original funciona);
2. caso contrário, para cada célula na borda dos anéis de raio 1 a 4 (passo 0,5) em volta da origem: descarta as bloqueadas (`0x00605C50`), escreve a candidata em `ebp-0x1C` e chama `0x006D9180` de novo com o mesmo destino;
3. primeira rota não vazia → `0x006D597A`; nenhuma → restaura a origem e segue para `0x006D593E`.

A DLL é um proxy de `version.dll` (o exe importa `VERSION.dll`) e só instala o hook se os 13 bytes forem exatamente os esperados.

## Resultados

**GF Violet (patch no exe, versão anterior):** 125 runs sem a falha na build corrigida; a build sem correção continuou falhando nos mesmos trajetos, em A/B com duas contas.

**Servidor 751 local (esta DLL):** com a DLL, nenhuma falha nos trajetos exercitados no Palácio de Proteção do Selo; sem a DLL, a falha voltou; reativada, não reapareceu. Não foi um teste exaustivo, porque a falha não tem trajeto fixo. Log dessa sessão:

```text
rota recuperada: origem (332.25,238.25) -> (331.75,237.75) raio 1 destino (254.25,196.25)
rota recuperada: origem (254.25,196.25) -> (253.75,195.75) raio 1 destino (334.25,241.25)
rota recuperada: origem (334.25,241.25) -> (333.75,240.75) raio 1 destino (278.25,177.25)
rota recuperada: origem (281.40,304.19) -> (280.90,303.69) raio 1 destino (334.25,246.25)
rota recuperada: origem (277.25,199.25) -> (276.75,198.75) raio 1 destino (443.25,266.25)
```

## Padrão observado

- Todas as falhas foram recuperadas já na **primeira candidata** (raio 1, deslocamento −0,5/−0,5); nenhuma precisou de anéis maiores.
- Em 4 de 5 casos a origem que falhou é **exatamente o destino da rota anterior**: o personagem parou no ponto de chegada e a próxima Autorrota partiu dali.
- Essas origens estão num alinhamento fixo da grade (coordenadas terminadas em `.25`).

Hipótese: quando o personagem para exatamente num ponto de destino da grade, o cálculo não aceita essa posição como início de rota (arredondamento ou borda de célula), e a célula vizinha resolve. Uma correção na raiz ficaria no próprio cálculo (`0x006D9180`) ou em como o ponto de chegada é gravado; o fallback contorna o problema sem depender disso.

## Limites

- Endereços válidos só para os executáveis indicados; em outros, confirme o fluxo antes (roteiro abaixo).
- Correção só do cliente. O servidor continua validando o movimento normalmente; não foi preciso alterá-lo.
- Destinos realmente inacessíveis continuam falhando, com no máximo 40 recálculos extras.

## Adaptar para outra build

1. Reproduza a falha e anote mapa, origem e destino; inclua um destino inacessível como controle.
2. Ache o ID local da mensagem em `T_TextIndex.ini` e rastreie no Ghidra (por referência ao ID ou em execução) até o ramo de falha.
3. Identifique a chamada de cálculo imediatamente anterior, os argumentos de origem e destino e o campo que indica rota vazia.
4. Identifique a validação de posição já usada pelo cliente.
5. Aplique o fallback só depois da primeira tentativa vazia, mantendo o destino e restaurando a origem em caso de falha.
6. Compare com e sem a correção nos mesmos trajetos, incluindo o destino inacessível e uma rota que já funciona.
