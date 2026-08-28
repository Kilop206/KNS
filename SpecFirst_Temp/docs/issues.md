# Issues

## Por que este arquivo existe

Este documento registra trabalho planejado em formato executavel por humanos e agentes. Ele e util quando o projeto ainda nao tem issue tracker ou quando voce quer manter um plano local canonico.

## Natureza de Template

Este arquivo e um template. No primeiro chat de escopo, revise e ajuste formato, regras de atualizacao, issues iniciais, status, fases, criterios de aceite e estado atual ao projeto real.

## Formato Recomendado

```md
## ISSUE-001 - [Titulo]

**Tipo:** Feature | Chore | Bug | Docs
**Epic:** [EPIC-XX]
**Status:** Planejada | Em andamento | Concluida | Bloqueada
**Fase:** [Fase do docs/implementation-plan.md]

### Objetivo

[O que esta issue entrega?]

### Criterios de aceite

- [criterio verificavel]

### Docs relevantes

- `docs/[arquivo].md`

### Estado atual

- **AAAA-MM-DD (IA):** [nota de progresso, bloqueio ou conclusao]
```

## Regras de Atualizacao

- Toda issue em execucao deve ter status `Em andamento`.
- Toda entrega concluida deve mudar o status para `Concluida`, salvo bloqueio explicito.
- O campo `### Estado atual` deve funcionar como log vivo da issue, com entradas datadas.
- Se a entrega alterar arquitetura, fluxo, dados ou seguranca, referencie os docs atualizados.
- Se a entrega gerar rastro tecnico, registre tambem em `docs/deployment-log.md`.

## ISSUE-001 - Fundacao documental

**Tipo:** Docs
**Epic:** EPIC-01
**Status:** Concluida
**Fase:** Fase 0 - Fundacao documental

### Objetivo

Criar contrato de trabalho e documentacao canonica inicial.

### Criterios de aceite

- `AGENTS.md` descreve proposito, regras, stack e fluxo de trabalho.
- `docs/architecture.md` define camadas e dependencias.
- `docs/testing.md` define checks esperados.
- `docs/decision-log.md` contem decisoes iniciais.
- `docs/ai-workflow.md` define sincronia obrigatoria entre issue, plano e log tecnico.
- `docs/deployment-log.md` define o rastro tecnico das entregas.
- `README.md` explica o fluxo Spec-First de adaptar documentacao antes de gerar codigo.
- `docs/tooling-adapters.md` explica adaptadores para Claude Code, Cursor, Windsurf e outras ferramentas.
- `docs/design-guidelines.md` define diretrizes de design para UI e frontend.

### Docs relevantes

- `AGENTS.md`
- `docs/project-overview.md`
- `docs/architecture.md`
- `docs/testing.md`

### Estado atual

- **2026-05-24 (IA):** [CONCLUIDA] Adicionada regra de kickoff visual: se o projeto tiver UI e o humano nao passar preferencias de design, a IA deve perguntar antes de implementar telas, componentes ou estilos.
- **2026-05-24 (IA):** [CONCLUIDA] Evoluido `docs/design-guidelines.md` para incluir baseline prescritivo de cores, tipografia, espacamento, layout, componentes, estados e acessibilidade, reduzindo improviso visual da IA em projetos com UI.
- **2026-05-24 (IA):** [CONCLUIDA] Reequilibrada a autonomia da IA para o modelo humano navegador/IA piloto, exigindo aprovacao humana para decisoes relevantes e remocao de documentos. Todos os arquivos em `docs/*`, alem de `AGENTS.md`, `CLAUDE.md` e `README.md`, agora explicitam sua natureza de template e o que deve ser ajustado ao escopo.
- **2026-05-22 (IA):** [CONCLUIDA] Reforcada a adaptacao inicial completa do framework no primeiro chat de escopo, incluindo `AGENTS.md`, `README.md`, adaptadores e todos os `docs/*`, com permissao para remover arquivos nao aplicaveis e limpar referencias.
- **2026-05-20 (IA):** [CONCLUIDA] Criado `docs/design-guidelines.md` e adicionadas referencias em README, AGENTS, indice de docs e workflow de IA para orientar implementacoes de UI.
- **2026-05-20 (IA):** [CONCLUIDA] Criado `CLAUDE.md` como adaptador operacional e documentada a estrategia de adaptadores em `docs/tooling-adapters.md`, mantendo `AGENTS.md` como fonte canonica.
- **2026-05-20 (IA):** [CONCLUIDA] Incluido o fluxo Spec-First com IA no README e em `docs/ai-workflow.md`, reforcando que a IA deve adaptar a documentacao ao escopo do projeto antes de criar codigo.
- **2026-05-20 (IA):** [CONCLUIDA] Fundacao documental ampliada com governanca de progresso, travamento de escopo, diferenca entre Decision Log e Deployment Log, Prompt Mestre de kickoff e rastro tecnico em `docs/deployment-log.md`.
