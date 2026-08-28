# Architecture

## Por que este arquivo existe

Este documento define a arquitetura autorizada do projeto. Ele deve guiar onde o codigo vive, quais camadas existem, quais dependencias sao permitidas e quais atalhos sao proibidos.

Quando alguem perguntar "onde isso deve ficar?", a resposta deve estar aqui.

## Natureza de Template

Este arquivo e um template. No primeiro chat de escopo, revise e ajuste visao geral, camadas, dependencias permitidas, dependencias proibidas e decisoes base ao projeto real.

## Visao Geral

`Contrato universal -> Documentacao canonica -> Projeto futuro`

## Camadas

### 1) Raiz do Projeto

**Local esperado:** `/README.md`, `/AGENTS.md`

Responsabilidades:

- explicar o que e o projeto;
- declarar regras obrigatorias;
- apontar para as rotas canonicas.

Nao deve:

- conter detalhes longos que pertencem a `docs/*`;
- duplicar conteudo detalhado da documentacao canonica.

### 2) Documentacao Canonica

**Local esperado:** `/docs`

Responsabilidades:

- registrar produto, arquitetura, workflow, padroes, testes e decisoes;
- manter os motivos das rotas e documentos;
- orientar evolucao do projeto.

Nao deve:

- virar deposito de notas soltas;
- contradizer `AGENTS.md`.

## Dependencias Permitidas

- `README.md` pode apontar para `AGENTS.md` e `docs/*`.
- `AGENTS.md` pode apontar para `docs/*` e definir regras de manutencao do framework.
- `docs/*` pode explicar o funcionamento do projeto.

## Dependencias Proibidas

- Docs canonicos nao devem duplicar integralmente conteudo de `AGENTS.md`.
- Arquivos de ferramenta, como `CLAUDE.md` ou `.cursorrules`, nao devem substituir `AGENTS.md`.

## Decisoes Base

- `AGENTS.md` e o contrato universal.
- `docs/*` e a fonte canonica tecnica e operacional.
- A estrutura raiz e o pacote copiavel para novos projetos.
