# CLAUDE.md

Este projeto usa SpecFirst.

Este arquivo e um adaptador operacional para Claude Code. Ele nao substitui nem duplica o contrato do projeto.

## Natureza de Template

Este arquivo e um template de adaptador. No primeiro chat de escopo, revise se Claude Code sera usado neste projeto.

Se nao for usado, solicite aprovacao humana antes de remover este arquivo e limpe suas referencias em `README.md`, `AGENTS.md`, `docs/README.md` e `docs/tooling-adapters.md`.

## Fonte Canonica

Antes de qualquer implementacao, leia obrigatoriamente:

1. `AGENTS.md`
2. `docs/README.md`
3. `docs/project-overview.md`
4. `docs/architecture.md`
5. `docs/ai-workflow.md`
6. `docs/coding-standards.md`
7. `docs/testing.md`

Em caso de conflito:

1. siga `AGENTS.md`;
2. depois siga `docs/*`;
3. por ultimo, use este arquivo apenas como adaptador do Claude Code.

## Regra Principal

Nao implemente codigo antes de adaptar ou validar a documentacao SpecFirst do projeto.

Quando receber um escopo novo:

1. leia o contrato em `AGENTS.md`;
2. revise `AGENTS.md`, `README.md`, este arquivo e todos os documentos em `docs/*`;
3. proponha ajustes ao projeto real;
4. solicite aprovacao humana antes de remover qualquer arquivo;
5. registre duvidas, riscos e decisoes pendentes;
6. aguarde aprovacao humana antes de iniciar a implementacao.

## Limite de Autonomia

Claude Code pode propor o como tecnico, mas nao deve decidir sozinho o que sera construido, por que sera construido, qual arquitetura sera adotada, quais documentos serao removidos ou quando o escopo deve mudar.

## Fechamento de Tarefa

Antes de considerar uma tarefa concluida:

1. atualize `docs/issues.md`;
2. atualize `docs/implementation-plan.md`;
3. registre a entrega em `docs/deployment-log.md`;
4. atualize `docs/decision-log.md` apenas quando houver decisao duradoura.

Este arquivo deve permanecer curto. Regras completas pertencem a `AGENTS.md` e `docs/*`.
