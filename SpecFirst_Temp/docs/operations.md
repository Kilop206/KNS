# Operations

## Por que este arquivo existe

Este documento explica como rodar, configurar, manter e publicar o projeto. Ele deve permitir que alguem opere o sistema sem depender de memoria oral.

## Natureza de Template

Este arquivo e um template. No primeiro chat de escopo, revise e ajuste ambiente local, variaveis, URLs, seeds, backup, checks e checklist de deploy ao projeto real.

## Ambiente Local

1. Instalar runtime: `[Node, Python, Docker, etc.]`
2. Instalar dependencias: `[comando]`
3. Criar arquivo de ambiente a partir de `[arquivo exemplo]`
4. Rodar projeto: `[comando]`

## Variaveis de Ambiente

| Variavel | Obrigatoria | Descricao |
| --- | --- | --- |
| `[VARIAVEL]` | Sim/Nao | `[descricao]` |

## URLs Locais

- App: `[URL]`
- Admin/API/Docs: `[URL]`

## Seeds, Fixtures ou Dados Iniciais

Explique se existe comando para popular dados.

```bash
[comando_seed]
```

## Backup e Restauracao

Explique o que precisa ser copiado para restaurar o projeto.

- Banco: `[estrategia]`
- Uploads/assets: `[estrategia]`
- Configuracao: `[estrategia]`

## Checks de Qualidade

```bash
[comando_lint]
[comando_typecheck]
[comando_test]
[comando_build]
```

## Checklist de Deploy

- `[item de deploy]`
- `[item de seguranca]`
- `[item de verificacao]`
