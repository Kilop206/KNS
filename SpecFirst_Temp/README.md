# SpecFirst

SpecFirst é um framework de documentação em Markdown para iniciar projetos com contexto claro antes da implementação crescer.

A proposta é simples: antes de escrever muito código, o projeto deve ter uma fonte de verdade mínima sobre objetivo, arquitetura, regras, validação, fluxo de trabalho e uso de IA. Essa fonte fica em `AGENTS.md` e `docs/*`, usando arquivos Markdown fáceis de copiar, versionar e adaptar.

O próprio repositório do SpecFirst funciona como template. Para usar em outro projeto, copie `AGENTS.md` e a pasta `docs`.

## Natureza de Template

Este README também é parte do template. Ao aplicar o SpecFirst em um projeto real, revise este guia para refletir o nome, o propósito, o fluxo, os documentos mantidos e os adaptadores usados naquele projeto.

Remova ou ajuste seções que não se aplicam, mas peça validação humana antes de excluir arquivos Markdown do framework.

## O que é o SpecFirst

SpecFirst pode ser entendido em três camadas complementares:

- **Framework documental:** define um modo de trabalhar, com regras, ciclo de vida, governança, logs, fluxo de IA, Definition of Done e controle de escopo.
- **Template Markdown:** entrega uma estrutura copiável baseada em `AGENTS.md` e `docs/*`, pronta para iniciar novos projetos.
- **Kit de governança para IA:** reúne documentos operacionais para manter plano, execução, decisões e histórico técnico sincronizados.

Por isso, a melhor definição curta é:

> SpecFirst é um framework documental em Markdown, distribuído como template, para governar projetos tocados por humanos e agentes de IA.

Ele não é apenas um conjunto de arquivos. O valor está no conceito: começar pela especificação, manter decisões rastreáveis e fazer a IA registrar o próprio avanço antes de encerrar uma entrega.

## Propósito

Muitos projetos começam com decisões espalhadas em conversas, prompts, issues, commits e memória das pessoas. Quando entram agentes de IA, automações ou novos colaboradores, esse contexto precisa ser reconstruído a cada tarefa.

SpecFirst resolve esse problema criando uma estrutura documental pequena, operacional e replicável. Ela define onde cada tipo de decisão deve viver e como humanos, agentes e ferramentas devem encontrar contexto antes de implementar.

SpecFirst ajuda a responder:

- qual problema o projeto resolve;
- quais regras não podem ser quebradas;
- qual arquitetura deve ser seguida;
- como a IA deve trabalhar no projeto;
- quais decisões já foram tomadas;
- qual issue e fase estão em andamento;
- o que foi tecnicamente entregue;
- como validar que uma entrega está pronta.

## Benefícios

### Contexto antes do código

SpecFirst reduz implementações impulsivas. O projeto começa com problema, escopo, arquitetura e critério de pronto minimamente descritos, evitando que a primeira solução técnica vire a arquitetura por acidente.

### Melhor colaboração com IA

Agentes de IA trabalham melhor quando existe um contrato claro. `AGENTS.md` informa regras obrigatórias, fluxo de trabalho e documentos relevantes. `docs/*` guarda o contexto detalhado que deve ser lido sob demanda.

### Menos conhecimento perdido

Decisões importantes deixam de ficar apenas em conversas ou na memória da equipe. O `decision-log.md` registra contexto, decisão e consequências, facilitando manutenção e onboarding.

### Escopo mais controlado

Arquivos como `implementation-plan.md`, `issues.md`, `backlog.md` e `implementation-governance.md` ajudam a separar ideia, plano, decisão e execução. Isso evita que uma tarefa pequena vire uma reescrita ampla sem combinação prévia.

### Validação mais objetiva

`testing.md`, `coding-standards.md` e a Definition of Done do `AGENTS.md` deixam claro quais checks devem ser executados e o que significa uma tarefa estar pronta.

### Sincronia entre plano e execução

SpecFirst torna explícito que a IA é responsável por manter o progresso documentado. Uma tarefa não termina apenas porque o código foi escrito: a issue precisa ser atualizada, o plano precisa refletir o avanço e o rastro técnico precisa ser registrado.

### Independência de ferramenta

SpecFirst não depende de uma plataforma específica. Ele pode ser usado com qualquer editor, agente de IA, ferramenta de automação, issue tracker ou stack técnica, porque o contrato vive em Markdown versionado.

### Compatibilidade com múltiplos agentes

O contrato universal fica em `AGENTS.md`, mas ferramentas diferentes podem ter portas de entrada próprias. Por isso, o SpecFirst permite adaptadores como `CLAUDE.md`, `.cursorrules` ou arquivos equivalentes.

Esses adaptadores não devem duplicar as regras do projeto. Eles apenas apontam a ferramenta para `AGENTS.md` e `docs/*`, mantendo uma única fonte de verdade.

## Quando Usar

Use SpecFirst quando você quer iniciar ou reorganizar um projeto que precisa de:

- contexto compartilhado entre pessoas e agentes de IA;
- documentação leve, mas operacional;
- regras claras antes de escalar implementação;
- arquitetura e decisões rastreáveis;
- um processo simples para repetir projetos futuros;
- onboarding mais rápido de colaboradores e ferramentas.

Ele funciona bem para produtos, apps internos, sites, bibliotecas, APIs, automações, projetos client-based e experimentos que podem evoluir para produto.

## Fluxo Spec-First com IA

A ideia central do SpecFirst é usar a IA primeiro para estruturar o projeto, não para sair programando.

A dinâmica correta é:

- **Humano como navegador:** define o que será feito, por que será feito, contexto de negócio, prioridades e decisões de produto, arquitetura e escopo.
- **IA como piloto:** propõe o como técnico, escreve código, gera testes, lida com boilerplate, executa refatorações mecânicas e mantém documentação sincronizada.

A IA não deve operar como dona do produto. Quando houver decisão relevante, ela deve propor opções, explicar tradeoffs e pedir validação humana.

Em vez de começar com um pedido como "crie um app de tarefas", o fluxo recomendado é:

1. **Start técnico:** crie um repositório vazio e copie `AGENTS.md` e `docs`.
2. **Contextualização:** descreva a ideia do produto para a IA e peça para ela ler `AGENTS.md`.
3. **Adaptação completa do SpecFirst:** peça para a IA revisar todos os arquivos do framework, incluindo `AGENTS.md`, `README.md`, adaptadores de ferramenta e `docs/*`.
4. **Sem código ainda:** a IA deve mapear escopo, entidades, regras, riscos, fases e critérios de aceite antes de criar arquivos de implementação.
5. **Poda assistida do template:** arquivos que não fazem sentido para o projeto podem ser propostos para remoção, mas a IA deve pedir aprovação humana antes de excluir qualquer Markdown.
6. **Kickoff visual, se houver UI:** se você não passou preferências de design, a IA deve perguntar sobre marca, referências, estilo, densidade, público, acessibilidade e restrições antes de criar telas.
7. **Revisão de intenção:** você revisa os Markdown gerados. Se a IA extrapolou o escopo, entendeu uma entidade errado ou pulou uma regra importante, corrija no texto.
8. **Green light:** só depois da documentação aprovada, peça para a IA implementar a primeira fase seguindo `AGENTS.md`, `docs/implementation-plan.md` e `docs/testing.md`.

Esse fluxo transforma a IA de geradora de código em parceira de produto e arquitetura. Ela ajuda a criar a própria forma onde o código será injetado depois.

Isso é superior ao "vibe coding" porque:

- reduz retrabalho ao corrigir escopo em Markdown antes de reescrever código;
- preserva contexto quando o chat estoura, a sessão muda ou o projeto pausa por semanas;
- cria guarda-corpos para a IA se auto-policiar durante a implementação;
- deixa claro o que está dentro e fora do escopo antes da primeira linha de código;
- torna o onboarding de outra IA ou pessoa muito mais rápido.

## Como Usar

1. Copie `AGENTS.md` para a raiz do novo projeto.
2. Copie `docs` para a raiz do novo projeto.
3. Substitua os marcadores entre colchetes, como `[NOME_DO_PROJETO]`.
4. Remova seções que não se aplicam.
5. Adicione documentos específicos apenas quando eles criarem clareza operacional.
6. Mantenha `AGENTS.md` curto o bastante para ser lido sempre.
7. Use `docs/*` para detalhes estáveis de produto, arquitetura e processo.
8. Se usar Claude Code, Cursor, Windsurf ou outra ferramenta com arquivo próprio de instruções, crie um adaptador curto seguindo `docs/tooling-adapters.md`.

## Prompt Mestre de Kickoff

Use este prompt ao iniciar um novo projeto com uma IA:

```text
Você é um agente trabalhando sob o framework SpecFirst.

Antes de implementar qualquer coisa:

1. Leia `AGENTS.md`.
2. Leia `README.md`, `docs/README.md` e todos os arquivos em `docs/*`.
3. A partir do escopo descrito pelo humano, adapte o framework inteiro para este projeto específico, incluindo `AGENTS.md`.
4. Preencha, revise ou proponha a remoção de documentos conforme aderência ao projeto.
5. Antes de remover qualquer arquivo Markdown, liste o motivo e peça aprovação humana explícita.
6. Se a remoção for aprovada, limpe todas as referências cruzadas em `README.md`, `AGENTS.md`, `docs/README.md` e documentos relacionados.
7. Não crie código ainda.
8. Se o projeto tiver UI e nenhuma preferência visual tiver sido informada, pergunte sobre marca, referências, estilo visual, densidade da interface, público, acessibilidade e restrições.
9. Confirme objetivo, critério de aceite, riscos, arquivos prováveis, checks necessários, documentos propostos para remoção e pontos que precisam de decisão humana.
10. Aguarde aprovação humana do escopo documentado antes de implementar.

Ao concluir:

1. Atualize o status e o histórico da issue em `docs/issues.md`.
2. Atualize o checklist ou fase em `docs/implementation-plan.md`.
3. Registre a entrega técnica em `docs/deployment-log.md`.
4. Atualize `docs/decision-log.md` apenas se houver decisão duradoura de arquitetura, produto, dados, segurança ou operação.
5. Resuma no chat os arquivos alterados, checks executados, docs atualizados e riscos residuais.
```

## Como Preencher

Comece pelo núcleo mínimo:

1. `AGENTS.md`: contrato universal do projeto, regras invioláveis e mapa de contexto.
2. `docs/project-overview.md`: problema, público, objetivo, escopo e fora de escopo.
3. `docs/architecture.md`: camadas, dependências permitidas e proibidas.
4. `docs/ai-workflow.md`: como agentes de IA devem explorar, implementar, validar e relatar.
5. `docs/coding-standards.md`: padrões técnicos e convenções de código.
6. `docs/testing.md`: estratégia de testes, comandos oficiais e critério de pronto.

Depois, preencha os documentos sob demanda:

- `domains.md` para fronteiras de responsabilidade.
- `data-model.md` para entidades, schemas, persistência e contratos.
- `design-guidelines.md` para direção visual, tokens, componentes, layout e experiência frontend.
- `security.md` para auth, permissões, secrets e dados sensíveis.
- `workflows.md` para jornadas operacionais ou fluxos de usuário.
- `operations.md` e `deploy.md` para ambiente, publicação e suporte.
- `decision-log.md` para decisões duradouras.
- `deployment-log.md` para entregas técnicas realizadas.
- `implementation-plan.md`, `issues.md` e `backlog.md` para execução.
- `implementation-governance.md` para travas de escopo e avanço de fase.
- `tooling-adapters.md` para compatibilidade com Claude Code, Cursor, Windsurf e outros agentes.
- `templates.md` para recipes e padrões reutilizáveis.

## Ciclo de Vida de uma Tarefa com IA

No SpecFirst, a IA deve manter sincronia entre plano e execução:

1. **Antes de implementar:** ler o contrato, identificar issue/fase, confirmar critério de aceite e riscos.
2. **Durante a implementação:** fazer o menor incremento seguro e validar com os checks definidos.
3. **Antes de encerrar:** atualizar `docs/issues.md`, `docs/implementation-plan.md` e `docs/deployment-log.md`.
4. **Quando houver decisão duradoura:** registrar também em `docs/decision-log.md`.

Essa regra evita que a documentação morra depois dos primeiros commits.

## Decision Log vs Deployment Log

Use `docs/decision-log.md` para registrar o motivo de decisões duradouras:

- escolha de stack;
- mudança arquitetural;
- alteração de modelo de dados;
- decisão de segurança;
- mudança de fluxo operacional.

Use `docs/deployment-log.md` para registrar o que foi tecnicamente entregue:

- arquivos modificados;
- issue relacionada;
- testes e checks executados;
- riscos e débitos técnicos;
- documentação atualizada.

## Estrutura Recomendada

```text
.
|-- README.md
|-- AGENTS.md
|-- CLAUDE.md
`-- docs
    |-- README.md
    |-- project-overview.md
    |-- architecture.md
    |-- ai-workflow.md
    |-- coding-standards.md
    `-- testing.md
```

Ao usar o SpecFirst em outro projeto, a estrutura completa pode ficar assim:

```text
.
|-- AGENTS.md
`-- docs
    |-- README.md
    |-- project-overview.md
    |-- architecture.md
    |-- ai-workflow.md
    |-- coding-standards.md
    |-- testing.md
    |-- domains.md
    |-- data-model.md
    |-- design-guidelines.md
    |-- security.md
    |-- workflows.md
    |-- operations.md
    |-- decision-log.md
    |-- deployment-log.md
    |-- implementation-governance.md
    |-- implementation-plan.md
    |-- issues.md
    |-- tooling-adapters.md
    `-- templates.md
```

## Adaptadores de Ferramenta

SpecFirst é agent-agnostic. Ele funciona com Codex, Claude Code, Cursor, Windsurf e outras ferramentas porque separa contrato de adaptador.

- `AGENTS.md` é o contrato universal.
- `docs/*` é a documentação canônica.
- `CLAUDE.md`, `.cursorrules` e similares são adaptadores operacionais.

Um adaptador deve ser curto e dizer para a ferramenta ler `AGENTS.md` e os docs nucleares. Ele não deve copiar todas as regras, porque isso cria divergência com o tempo.

## Princípio do Framework

`AGENTS.md` é o contrato universal. Ele deve ser curto, obrigatório e operacional. Seu papel é dizer quais regras importam, qual fluxo seguir e onde encontrar contexto.

`docs` é a documentação canônica. Ela deve ser viva, objetiva e útil para decisões futuras. Cada arquivo deve existir por um motivo claro.

O princípio central é: documentar o suficiente para orientar boas decisões, sem criar burocracia que atrapalhe a entrega.

Todos os arquivos do SpecFirst são templates vivos. Eles devem ser ajustados quando o escopo muda, quando o projeto evolui ou quando uma decisão aprovada altera produto, arquitetura, operação, segurança, design ou fluxo de trabalho.

## Filosofia

SpecFirst não significa documentar tudo antes de agir. Significa documentar primeiro os contratos que reduzem retrabalho:

- objetivo;
- fronteiras;
- regras;
- decisões;
- validação;
- fluxo de trabalho.

O resultado esperado é um projeto que agentes e pessoas conseguem entender rapidamente, modificar com mais segurança e evoluir sem depender de contexto escondido.

A IA não é apenas executora de código. Dentro do SpecFirst, ela também é responsável por manter o histórico de engenharia sincronizado com o que acabou de entregar.
