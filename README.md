# autorrota-fix

Correção **do lado do cliente** para a Autorrota de Grand Fantasia (cliente TW `006.750.79.79`, base dos servidores 751) quando ela exibe **“Não pode ser movido para o lugar de uso”** em trajetos alcançáveis.

*English summary below.*

## O que faz

Quando o cálculo de rota do cliente devolve uma rota vazia, a DLL repete o **mesmo cálculo original** a partir de células transitáveis próximas da origem (bordas de anéis de raio 1 a 4, passo 0,5), mantendo o **mesmo destino**. A primeira rota válida segue o fluxo normal; se nenhuma servir, a origem é restaurada e o cliente mostra a falha original.

Não teleporta, não ignora colisão e não altera mapas, destino, servidor ou o executável em disco.

## Como funciona

- É uma `version.dll` “proxy”: o `GrandFantasia.exe` importa `VERSION.dll`, então o Windows carrega esta DLL da pasta do jogo. Todas as funções de `version.dll` são encaminhadas para a DLL do sistema.
- Ao carregar, ela confere os 13 bytes originais em `0x006D5931` (ponto de decisão após o cálculo de rota). **Se não forem os esperados, não altera nada** e registra isso no log.
- Com o exe reconhecido, instala um `jmp` em memória para a rotina de fallback em [`src/autoroute_fix.c`](src/autoroute_fix.c).

Detalhes, endereços e evidências: [`docs/nota-tecnica.md`](docs/nota-tecnica.md).

## Uso

1. Baixe `version.dll` da [release](../../releases) (gerada pelo GitHub Actions a partir deste código, com SHA-256 publicado) ou compile com `build.bat` (Visual Studio 2022 / Build Tools).
2. Copie `version.dll` para a pasta do `GrandFantasia.exe`.
3. Abra o jogo normalmente. O arquivo `autoroute_fix.log` na mesma pasta mostra se o hook foi instalado e cada rota recuperada.

Para desativar, remova ou renomeie `version.dll`.

### Antes de copiar: já existe `version.dll` na pasta?

Esta DLL **não é uma cópia alterada** da `version.dll` do Windows (que continua intacta em `System32`); é um arquivo novo que só existe na pasta do jogo. Normalmente não há nenhuma `version.dll` ali.

Se já houver uma, ela pertence a outro mod ou ao loader do seu servidor, e **substituí-la vai desativá-lo**. Nesse caso não copie por cima: integre a função `RetryWithNearbyOrigin` e o hook de [`src/autoroute_fix.c`](src/autoroute_fix.c) no loader existente, ou peça isso a quem mantém o cliente.

## Compatibilidade

| Executável | SHA-256 | Estado |
|---|---|---|
| TW `006.750.79.79` (cliente dos pacotes 751) | `B43E2AF3CABE32439FEFB458C9EDC38252C9F4BC10CD2301D8642990740EFBB1` | testado |
| Mesmo exe com recursos/loader alterados (ex.: GF Violet) | — | mesmos endereços; a verificação de bytes decide |

Outros executáveis: a DLL não atua. O método para adaptar está na nota técnica.

Clientes que já carregam outra `version.dll` ou um loader próprio podem integrar a mesma função no seu código em vez de usar o proxy.

---

## English

Client-side fix for Grand Fantasia's auto-route (“cannot move to the destination”) on the TW `006.750.79.79` client used by 751 private servers. When the original path search returns an empty route, it retries the **same** search from walkable cells near the origin (square rings, radius 1–4, 0.5 step), keeping the destination. Loaded as a `version.dll` proxy; verifies the original bytes before hooking and does nothing on unknown executables. No teleport, collision, map, server or on-disk exe changes. See `docs/nota-tecnica.md` (Portuguese) for addresses and evidence.

## Licença

MIT. Este repositório contém apenas código e documentação próprios; nenhum arquivo do jogo.
