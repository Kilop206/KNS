# Decision Log

## Por que este arquivo existe

Este documento registra decisoes que mudam arquitetura, modelo, fluxo, escopo ou operacao. Ele evita que o motivo de uma escolha desapareca com o tempo.

Nao use este arquivo como historico de entregas tecnicas. Para isso, use `docs/deployment-log.md`.

## Natureza de Template

Este arquivo e um template. No primeiro chat de escopo, revise decisoes iniciais, mantenha apenas as que fizerem sentido para o projeto real e registre novas decisoes quando o humano validar uma escolha duradoura.

## Como Usar

- Identificador incremental: `0001`, `0002`, `0003`.
- Estado: `Proposta`, `Aceita`, `Substituida` ou `Rejeitada`.
- Cada decisao deve listar contexto, decisao e consequencias.
- Decisoes substituidas devem apontar para a decisao nova.

## Template de Decisao

```md
## 0001 - [Titulo curto]

- **Data:** AAAA-MM-DD
- **Estado:** Proposta

### Contexto

[Qual problema, conflito ou oportunidade motivou a decisao?]

### Decisao

[O que foi decidido?]

### Consequencias

- [Impacto positivo]
- [Tradeoff]
- [O que precisa mudar agora]
```

## 0001 - Adotar AGENTS.md como contrato universal

- **Data:** 2026-05-19
- **Estado:** Aceita

### Contexto

O projeto pode ser trabalhado por pessoas, agentes e ferramentas diferentes. Sem uma fonte comum, regras e arquitetura podem divergir.

### Decisao

Adotar `AGENTS.md` como contrato universal e `docs/*` como fonte canonica tecnica e de produto.

### Consequencias

- Menos dependencia de uma ferramenta especifica.
- Maior consistencia entre entregas.
- Necessidade de manter documentacao viva.

## 0002 - Sincronizar plano, issues e log tecnico

- **Data:** 2026-05-20
- **Estado:** Aceita

### Contexto

Projetos tocados por IA perdem governanca quando a implementacao avanca mas `docs/issues.md`, `docs/implementation-plan.md` e o historico tecnico ficam desatualizados.

### Decisao

Tornar obrigatoria a sincronizacao entre issue, plano de implementacao e `docs/deployment-log.md` antes de qualquer tarefa ser considerada concluida.

### Consequencias

- A IA passa a ser responsavel por registrar o proprio avanco.
- O fechamento de tarefa deixa de ser apenas relato no chat e passa a persistir historico no repositorio.
- `docs/decision-log.md` fica reservado para decisoes duradouras, enquanto `docs/deployment-log.md` registra entregas tecnicas.
