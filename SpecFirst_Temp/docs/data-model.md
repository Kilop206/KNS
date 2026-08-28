# Data Model

## Por que este arquivo existe

Este documento descreve os dados do projeto antes que eles virem implementacao espalhada. Ele deve explicar entidades, campos, relacoes, validacoes e fallbacks.

Use quando alterar banco, schemas, collections, APIs, contratos ou modelos persistidos.

## Natureza de Template

Este arquivo e um template. No primeiro chat de escopo, revise e ajuste entidades, campos, relacoes, regras, validacoes, migracoes e evolucao ao projeto real.

## Principios

- Dados devem representar conceitos do dominio, nao atalhos de UI.
- Campos obrigatorios devem ter motivo claro.
- Dados opcionais devem ter fallback previsivel.
- Slugs, URLs, emails, ids externos e enums devem ser validados.

## Entidade: `[NOME_DA_ENTIDADE]`

Uso: `[para que esta entidade existe]`

Campos:

- `[campo]`: `[tipo, obrigatoriedade, descricao]`

Relacoes:

- `[relacao com outra entidade]`

Regras:

- `[validacao ou regra de negocio]`

## Migracoes e Evolucao

Explique como mudancas de modelo devem ser feitas.

- Quando criar migracao.
- Quando atualizar seeds.
- Quando atualizar tipos gerados.
- Quando registrar decisao em `docs/decision-log.md`.
