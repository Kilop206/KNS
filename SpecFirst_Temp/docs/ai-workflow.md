# AI Workflow

## Por que este arquivo existe

Este documento define como agentes de IA devem trabalhar no projeto. Ele reduz entregas impulsivas e cria um fluxo previsivel de contexto, implementacao, validacao e documentacao.

## Natureza de Template

Este arquivo e um template. No primeiro chat de escopo, revise e ajuste principio, fluxo operacional, inicializacao, limites de autonomia, kickoff, fechamento e diferenca entre logs ao projeto real.

## Principio

- Humano = navegador: define o que sera feito, por que sera feito, prioridade, contexto de negocio e decisoes de produto, arquitetura e escopo.
- IA = piloto: define o como tecnico, escreve codigo, gera testes, lida com boilerplate, executa refatoracoes mecanicas e mantem documentacao sincronizada.

A IA deve propor, explicar tradeoffs e pedir decisao quando houver mudanca relevante. Ela nao deve agir como dona do produto nem tomar decisoes de escopo sozinha.

## Fluxo Operacional

1. **Especificacao antes do codigo**
   - Adaptar `AGENTS.md` e `docs/*` ao escopo real do projeto antes de criar implementacao.
2. **Intencao antes de implementar**
   - Definir objetivo, restricoes e criterio de pronto.
3. **Contexto minimo**
   - Ler `AGENTS.md` e docs relevantes.
4. **Contrato antes da UI ou automacao**
   - Confirmar dados, entradas, saidas e fronteiras.
5. **Teste antes do codigo**
   - Criar teste quando houver comportamento novo ou contrato importante.
6. **Pequenas entregas**
   - Implementar incrementos curtos, revisaveis e reversiveis.
7. **Validacao**
   - Rodar checks aplicaveis.
8. **Sincronia de progresso**
   - Atualizar issue, plano e log tecnico antes de considerar a tarefa pronta.
9. **Documentacao viva**
   - Atualizar docs quando arquitetura, modelo ou fluxo mudar.

## Inicializacao de Projeto

Quando o SpecFirst for copiado para um projeto novo, a primeira tarefa da IA nao e criar codigo. A primeira tarefa e transformar o framework inteiro em documentacao especifica do projeto.

A IA deve:

1. ler `AGENTS.md`, `README.md`, adaptadores de ferramenta e `docs/README.md`;
2. receber do humano o escopo inicial do produto, app, API, site, biblioteca ou automacao;
3. revisar todos os arquivos de `docs/*`, nao apenas os nucleares;
4. propor ajustes em `AGENTS.md` ao projeto real, incluindo proposito, regras, stack, estrutura e Definition of Done;
5. propor preenchimento de escopo, fora de escopo, dominios, dados, riscos, fases e criterios de aceite;
6. identificar documentos que talvez nao se apliquem ao projeto e pedir aprovacao humana antes de remove-los;
7. se a remocao for aprovada, limpar referencias para arquivos removidos em `README.md`, `AGENTS.md`, `docs/README.md` e documentos relacionados;
8. registrar duvidas, suposicoes ou decisoes pendentes;
9. aguardar validacao humana antes de iniciar implementacao.

Arquivos normalmente ajustados nessa etapa:

- `AGENTS.md`;
- `README.md`;
- `CLAUDE.md` ou outros adaptadores, quando existirem;
- `docs/README.md`;
- `docs/project-overview.md`;
- `docs/architecture.md`;
- `docs/domains.md`;
- `docs/data-model.md`;
- `docs/design-guidelines.md`, quando houver UI, marca, frontend ou experiencia visual;
- `docs/security.md`, quando houver auth, permissoes, secrets ou dados sensiveis;
- `docs/workflows.md`;
- `docs/implementation-plan.md`;
- `docs/issues.md`;
- `docs/testing.md`.

Arquivos que podem ser propostos para remocao quando nao fizerem sentido:

- `docs/editor.md`, se o projeto nao tiver conteudo editavel ou CMS;
- `docs/pdf-export.md`, se o projeto nao gerar documentos imprimiveis;
- `docs/new-client-workflow.md`, se o projeto nao for replicado por cliente ou instancia;
- `docs/client-launch-checklist.md`, se nao houver entrega para cliente, area interna ou operador final;
- qualquer outro documento cuja manutencao gere ruido maior que valor.

Se a IA notar que precisa inventar produto, regra de negocio, entidade ou fluxo sem informacao suficiente, deve registrar a suposicao e pedir decisao humana.

## Limites de Autonomia

A IA pode executar sem nova aprovacao quando a tarefa for mecanica, local e ja estiver coberta pelo escopo aprovado.

A IA deve pedir aprovacao humana antes de:

- alterar escopo;
- escolher ou trocar arquitetura;
- criar ou remover modulo relevante;
- mudar modelo de dados;
- alterar regras de seguranca;
- definir direcao visual de projeto com UI quando `docs/design-guidelines.md` nao estiver preenchido com preferencias humanas;
- remover documentos do framework;
- pular fases ou reordenar o plano;
- adicionar dependencia nova;
- executar refatoracao ampla.

## Kickoff de Tarefa

Antes de implementar, a IA deve registrar:

- objetivo;
- issue ou fase relacionada;
- docs lidos;
- criterio de aceite;
- impacto em dados, seguranca, UI e testes;
- impacto em design, tokens, componentes e responsividade, quando houver UI;
- riscos ou suposicoes.

## Fechamento de Tarefa

Antes de encerrar a tarefa, commitar ou responder como concluida, a IA deve persistir o progresso no repositorio:

1. Atualizar `docs/issues.md`.
   - Mudar o status da issue quando aplicavel: `Planejada`, `Em andamento`, `Concluida` ou `Bloqueada`.
   - Registrar uma nota datada em `### Estado atual`.
2. Atualizar `docs/implementation-plan.md`.
   - Marcar checklists concluidos.
   - Atualizar a fase atual quando a entrega destravar a proxima etapa.
   - Nao pular fases com pendencias abertas sem decisao humana.
3. Atualizar `docs/deployment-log.md`.
   - Registrar o que foi feito, arquivos modificados, checks executados e riscos tecnicos.
4. Atualizar `docs/decision-log.md` apenas quando houver decisao de arquitetura, produto, modelo, seguranca ou operacao.

Depois de persistir o progresso, a IA deve relatar no chat:

- arquivos alterados;
- comportamento entregue;
- testes ou checks executados;
- status atualizado da issue;
- fase ou checklist atualizado no plano;
- entrada criada no log tecnico;
- pendencias ou riscos residuais;
- docs atualizados.

## Diferenca entre Logs

- `docs/decision-log.md` registra decisoes duradouras e seus motivos.
- `docs/deployment-log.md` registra entregas tecnicas realizadas.
- `docs/issues.md` registra o estado vivo de cada trabalho planejado.

Nao misture decisao arquitetural com historico operacional. Se uma entrega tecnica tambem gerar uma decisao duradoura, registre nos dois lugares com propositos diferentes.
