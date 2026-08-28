# AGENTS.md

## Por que este arquivo existe

Este arquivo e o contrato universal do projeto para pessoas, agentes de IA e ferramentas automatizadas. Ele deve ser lido antes de qualquer implementacao relevante.

Ele nao substitui a documentacao completa. Ele resume as regras obrigatorias, a arquitetura esperada e o caminho para encontrar contexto nos documentos certos.

## Natureza de Template

Este arquivo e um template. No primeiro chat de escopo, ele deve ser revisado e ajustado ao projeto real.

Adapte especialmente: proposito, regras inviolaveis, stack, estrutura de pastas, fluxo de trabalho, Definition of Done e referencias de documentacao.

## Proposito do Projeto

- **Nome:** `SpecFirst`
- **Tipo:** framework de documentacao em Markdown para projetos.
- **Problema resolvido:** projetos iniciam sem contrato claro de contexto, arquitetura, validacao e fluxo de trabalho para humanos e agentes de IA.
- **Publico-alvo:** pessoas desenvolvedoras, product builders, equipes pequenas, agentes de IA e ferramentas automatizadas que precisam operar sobre um projeto com regras claras.
- **Resultado esperado:** qualquer novo projeto pode copiar `AGENTS.md` e `docs` para nascer com documentacao minima, criterio de pronto e trilha de contexto.

## Regras Inviolaveis

Liste regras que nenhum agente ou desenvolvedor deve quebrar sem registrar uma decisao.

Exemplos:

1. Nunca criar codigo fora da arquitetura definida em `docs/architecture.md`.
2. Todo comportamento novo relevante deve ter teste.
3. Nenhum commit pode quebrar lint, testes ou typecheck.
4. Entradas externas devem ser validadas na fronteira apropriada.
5. Decisoes arquiteturais devem ser registradas em `docs/decision-log.md`.

Regras do SpecFirst:

1. No primeiro chat de escopo de um projeto novo, a IA deve revisar todos os arquivos do framework com o humano e propor ajustes ao projeto real, incluindo `AGENTS.md`, `README.md`, adaptadores de ferramenta e `docs/*`.
2. Arquivos que nao se aplicam ao projeto podem ser removidos somente apos aprovacao humana explicita, desde que suas referencias sejam limpas de `README.md`, `AGENTS.md`, `docs/README.md` e documentos relacionados.
3. Toda nova rota de documentacao deve ser refletida em `README.md`, `AGENTS.md` e `docs/README.md`.
4. A IA e responsavel pelo progresso: nenhuma tarefa e considerada concluida sem atualizar o status correspondente em `docs/issues.md`, o checklist ou fase atual em `docs/implementation-plan.md` e o rastro tecnico em `docs/deployment-log.md`.
5. Arquivos especificos de ferramenta, como `CLAUDE.md`, `.cursorrules` ou similares, sao adaptadores operacionais. Eles nao devem duplicar nem contradizer `AGENTS.md`.
6. Toda documentacao pode evoluir quando o escopo ou o projeto mudar, desde que mudancas relevantes sejam propostas pela IA e validadas pelo humano.

## Stack do Projeto

Explique as tecnologias principais e suas versoes quando isso afetar decisoes.

- Linguagem: Markdown.
- Framework: estrutura documental propria baseada em `AGENTS.md + docs`.
- Banco: nao aplicavel.
- UI: nao aplicavel.
- Testes: revisao estrutural dos links, rotas e consistencia dos documentos.
- Deploy: distribuicao por copia de `AGENTS.md` e `docs` ou publicacao do repositorio.

## Estrutura de Pastas Esperada

- `/README.md` -> guia de uso do SpecFirst.
- `/AGENTS.md` -> contrato universal copiavel para novos projetos.
- `/CLAUDE.md` -> adaptador operacional para Claude Code.
- `/docs` -> documentacao canonica copiavel.

## Fluxo de Trabalho Esperado

Todo trabalho relevante deve seguir este ciclo:

1. Entender objetivo, contexto operacional e criterio de pronto.
2. Em projeto novo, propor a adaptacao do framework inteiro ao escopo e aguardar validacao humana antes de implementar.
3. Ler `AGENTS.md` e docs relevantes.
4. Escrever ou ajustar testes quando houver comportamento novo.
5. Implementar o menor incremento seguro.
6. Refatorar quando houver duplicacao real ou complexidade desnecessaria.
7. Validar com os checks definidos em `docs/testing.md`.
8. Sincronizar progresso em `docs/issues.md`, `docs/implementation-plan.md` e `docs/deployment-log.md`.
9. Atualizar docs quando arquitetura, modelo ou fluxo mudar.

## Definition of Done

Uma tarefa so esta pronta quando:

- o comportamento solicitado foi implementado;
- testes relevantes foram criados ou atualizados;
- tipagem, lint e checks aplicaveis foram executados;
- entradas externas estao validadas;
- nao ha duplicacao evidente;
- a arquitetura foi respeitada;
- `docs/issues.md` registra o status e o historico da issue;
- `docs/implementation-plan.md` reflete o avanco da fase ou checklist;
- `docs/deployment-log.md` registra o rastro tecnico da entrega;
- documentacao foi atualizada quando necessario;
- impossibilidades ou riscos residuais foram registrados.

## Referencias de Documentacao

### Nucleo minimo

Ler sempre antes de implementacoes relevantes:

- `docs/project-overview.md`
- `docs/README.md`
- `docs/architecture.md`
- `docs/ai-workflow.md`
- `docs/coding-standards.md`
- `docs/testing.md`

### Ler sob demanda

| Documento | Quando consultar |
| --- | --- |
| `docs/domains.md` | Ao alterar fronteiras de responsabilidade. |
| `docs/data-model.md` | Ao alterar dados, schemas, entidades ou persistencia. |
| `docs/design-guidelines.md` | Ao criar ou alterar UI, estilo visual, componentes, layout, marca ou experiencia frontend. |
| `docs/security.md` | Ao tocar auth, permissoes, uploads, secrets ou dados sensiveis. |
| `docs/workflows.md` | Ao alterar jornada operacional ou fluxo de usuario. |
| `docs/operations.md` | Ao mexer em ambiente, deploy, backups ou operacao. |
| `docs/templates.md` | Ao criar padroes reutilizaveis de UI, codigo ou documento. |
| `docs/tooling-adapters.md` | Ao criar ou revisar adaptadores para Claude Code, Cursor, Windsurf ou outras ferramentas. |
| `docs/decision-log.md` | Quando houver decisao arquitetural nova ou conflito entre regras. |
| `docs/deployment-log.md` | Ao registrar o que foi tecnicamente entregue, arquivos alterados, checks e riscos. |
| `docs/implementation-plan.md` | Ao planejar fases, sprints ou sequencia de entrega. |
| `docs/implementation-governance.md` | Ao controlar escopo, travas de fase e autorizacoes humanas. |
| `docs/issues.md` | Ao executar trabalho planejado por issue. |

## Hierarquia de Contexto

1. `AGENTS.md` e o contrato universal.
2. `docs/*` e a fonte canonica tecnica e de produto.
3. Arquivos de ferramenta, como `CLAUDE.md`, `.cursorrules` ou similares, sao adaptadores operacionais.
4. Em caso de conflito, seguir `AGENTS.md`, docs canonicos e `docs/decision-log.md`.

## Diretrizes de Uso de IA

O humano atua como navegador: define o que sera feito, por que sera feito, prioridades, contexto de negocio e decisoes de arquitetura, produto e escopo.

A IA atua como piloto: propoe caminhos tecnicos, escreve codigo, gera testes, lida com boilerplate, executa refatoracoes mecanicas e mantem a documentacao sincronizada.

Nunca:

- implementar sem entender o objetivo;
- criar arquitetura complexa sem necessidade comprovada;
- esconder tradeoffs ou riscos;
- deixar regras importantes apenas na memoria da ferramenta.
- decidir sozinha mudancas de escopo, arquitetura, produto, modelo de dados, seguranca ou remocao de documentos.
- remover arquivos do framework sem aprovacao humana explicita.

Sempre:

- simplificar solucoes;
- manter entregas pequenas;
- preferir contratos claros;
- pedir decisao humana quando houver mais de um caminho relevante;
- apresentar plano curto antes de executar trabalho amplo;
- validar antes de concluir;
- sincronizar issue, plano e log tecnico antes de encerrar uma tarefa;
- atualizar docs quando a decisao mudar.
