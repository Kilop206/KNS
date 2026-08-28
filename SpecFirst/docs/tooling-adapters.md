# Tooling Adapters

## Por que este arquivo existe

Este documento explica como conectar o SpecFirst a diferentes agentes e ferramentas sem duplicar regras.

SpecFirst usa `AGENTS.md` como contrato universal, mas algumas ferramentas leem arquivos proprios, como `CLAUDE.md`, `.cursorrules`, `.windsurfrules` ou equivalentes. Esses arquivos devem ser tratados como adaptadores operacionais.

## Natureza de Template

Este arquivo e um template. No primeiro chat de escopo, revise quais ferramentas serao usadas; mantenha apenas adaptadores relevantes e solicite aprovacao humana antes de remover arquivos como `CLAUDE.md`.

## Principio

`AGENTS.md` e a fonte canonica.

`docs/*` e a documentacao canonica de produto, arquitetura, processo e governanca.

Arquivos especificos de ferramenta sao apenas portas de entrada para lembrar aquela ferramenta de ler o contrato certo.

## Hierarquia

1. `AGENTS.md`
2. `docs/*`
3. Adaptadores de ferramenta, como `CLAUDE.md`, `.cursorrules`, `.windsurfrules` ou similares

Em caso de conflito, siga sempre `AGENTS.md` e `docs/*`.

## Regras para Adaptadores

- Devem ser curtos.
- Devem apontar para `AGENTS.md` e docs nucleares.
- Nao devem copiar regras completas do projeto.
- Nao devem contradizer `AGENTS.md`.
- Devem existir apenas quando uma ferramenta se beneficiar de um arquivo proprio.
- Devem reforcar que a IA nao deve criar codigo antes de adaptar ou validar o SpecFirst do projeto.

## Adaptador para Claude Code

Use `CLAUDE.md` na raiz do projeto.

Conteudo recomendado:

```md
# CLAUDE.md

Este projeto usa SpecFirst.

Antes de qualquer implementacao, leia:

1. `AGENTS.md`
2. `docs/README.md`
3. `docs/project-overview.md`
4. `docs/architecture.md`
5. `docs/ai-workflow.md`
6. `docs/coding-standards.md`
7. `docs/testing.md`

`AGENTS.md` e a fonte canonica. Em caso de conflito, siga `AGENTS.md`, depois `docs/*`, depois este arquivo.

Nao implemente codigo antes de adaptar ou validar a documentacao SpecFirst do projeto.
```

## Adaptador para Cursor

Se o projeto usar Cursor, crie `.cursorrules` apenas como ponte para o contrato principal.

Conteudo recomendado:

```text
Este projeto usa SpecFirst.

Leia AGENTS.md antes de implementar.
Leia docs/README.md para encontrar a documentacao canonica.

AGENTS.md e docs/* prevalecem sobre este arquivo.
Nao crie codigo antes de validar o escopo documentado.
```

## Adaptador para outras ferramentas

Para qualquer ferramenta nova:

1. descubra qual arquivo ela le automaticamente;
2. crie um adaptador curto;
3. aponte para `AGENTS.md` e docs nucleares;
4. nao duplique o contrato;
5. atualize este documento se o adaptador virar padrao do projeto.

## Antipadrao

Nao mantenha regras completas em varios arquivos.

Duplicar regras em `AGENTS.md`, `CLAUDE.md`, `.cursorrules` e outros adaptadores cria divergencia. O objetivo e ter muitas portas de entrada, mas uma unica fonte de verdade.
