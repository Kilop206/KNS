# Coding Standards

## Por que este arquivo existe

Este documento transforma preferencia tecnica em padrao explicito. Ele deve reduzir discussoes repetidas e orientar agentes a escrever codigo consistente com o projeto.

## Natureza de Template

Este arquivo e um template. No primeiro chat de escopo, revise e ajuste linguagem, framework, organizacao, validacao, reutilizacao, comentarios e checklist de PR ao projeto real.

## Linguagem e Tipagem

Defina regras da linguagem principal.

- `[ex: usar TypeScript strict]`
- `[ex: evitar any sem justificativa]`
- `[ex: tipar funcoes publicas]`

## Framework

Declare padroes especificos do framework.

- `[ex: Server Components sao o padrao]`
- `[ex: services nao devem conhecer detalhes de UI]`

## Organizacao

Explique onde criar arquivos.

- `[caminho]` -> `[responsabilidade]`

## Validacao

Defina como entradas externas devem ser validadas.

- `[ex: Zod em APIs publicas]`
- `[ex: validators nativos do framework no CMS]`

## Reutilizacao

- Antes de criar helper novo, buscar padrao existente.
- Extrair logica quando houver repeticao real.
- Nao criar abstracao antecipada para um unico uso.

## Comentarios

- Comentarios devem explicar motivo, tradeoff ou contexto.
- Evitar comentar o obvio.
- Decisoes arquiteturais devem ir para `docs/decision-log.md`.

## Checklist de PR

- Tipagem correta.
- Validacao aplicada nas fronteiras.
- Sem duplicacao evidente.
- Testes e checks relevantes executados.
- Docs atualizados quando necessario.
