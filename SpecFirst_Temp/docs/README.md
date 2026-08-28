# Docs - SpecFirst

## Por que este arquivo existe

Este indice organiza a documentacao canonica do SpecFirst. A raiz do repositorio e o proprio template que deve ser copiado para outros projetos.

## Natureza de Template

Este arquivo e um template. No primeiro chat de escopo, revise e ajuste este indice para refletir apenas os documentos usados pelo projeto real.

Se algum documento for removido com aprovacao humana, remova tambem sua entrada deste indice.

## Rotas principais

- `README.md` -> guia de uso do SpecFirst.
- `AGENTS.md` -> contrato universal generico para novos projetos.
- `docs/*` -> documentacao canonica generica.

## Documentos nucleares

- `project-overview.md` -> problema, publico, objetivo e escopo do projeto.
- `architecture.md` -> arquitetura documental e rotas autorizadas.
- `ai-workflow.md` -> como agentes devem trabalhar no projeto.
- `coding-standards.md` -> padroes para manter a documentacao consistente.
- `testing.md` -> validacoes esperadas para mudancas no framework.
- `design-guidelines.md` -> direcao visual, tokens, componentes, layout e regras para UI.
- `implementation-plan.md` -> fases, checklists e criterios globais de aceite.
- `issues.md` -> trabalho planejado e estado vivo de cada issue.
- `deployment-log.md` -> historico tecnico das entregas realizadas.
- `tooling-adapters.md` -> como conectar SpecFirst a Claude Code, Cursor, Windsurf e outros agentes sem duplicar regras.

## Fronteira entre Historicos

- `decision-log.md` guarda o motivo de decisoes duradouras.
- `deployment-log.md` guarda o que foi tecnicamente entregue.
- `issues.md` guarda o status e o progresso operacional de cada tarefa.

## Regra

Quando uma rota ou documento novo entrar no framework, atualize este indice, `README.md` e `AGENTS.md`.

No primeiro chat de escopo de um projeto novo, a IA deve revisar todos os arquivos do framework e adaptar esta lista ao projeto real. Documentos que nao se aplicam podem ser removidos, desde que suas referencias tambem sejam removidas dos indices e documentos relacionados.

Todos os documentos podem ser atualizados quando o escopo mudar ou quando o projeto evoluir. A IA deve propor a atualizacao, explicar o motivo e aguardar decisao humana quando a mudanca afetar produto, arquitetura, seguranca, dados, operacao ou remocao de arquivos.
