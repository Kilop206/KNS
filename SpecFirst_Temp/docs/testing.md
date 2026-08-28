# Testing

## Por que este arquivo existe

Este documento define como o projeto prova que continua funcionando. Ele deve listar estrategia, tipos de teste, comandos esperados e o minimo necessario para considerar uma tarefa pronta.

## Natureza de Template

Este arquivo e um template. No primeiro chat de escopo, revise e ajuste estrategia, tipos de teste, comandos oficiais, regras e Definition of Done ao projeto real.

## Estrategia

Adote uma estrategia clara, como Red-Green-Refactor para comportamento novo:

1. escrever teste que descreve o comportamento esperado;
2. implementar o minimo para passar;
3. refatorar sem mudar comportamento.

## Tipos de Teste

Adapte a lista ao projeto:

- **Unit tests:** helpers, validadores, formatadores e regras puras.
- **Integration tests:** fronteiras entre modulos, banco, APIs ou servicos externos simulados.
- **Component tests:** componentes com contratos relevantes.
- **E2E ou smoke tests:** fluxos criticos e rotas principais.

## Comandos Esperados

Defina os comandos oficiais.

```bash
[comando_lint]
[comando_typecheck]
[comando_test]
[comando_build]
```

## Regras

- Codigo novo relevante exige teste.
- Bug corrigido deve ganhar teste de regressao quando possivel.
- Nao fechar tarefa com teste quebrado.
- Quando um check nao puder rodar, registrar motivo e risco.

## Definition of Done

Uma tarefa esta concluida quando:

- comportamento implementado;
- testes relevantes criados ou atualizados;
- checks aplicaveis executados;
- docs ajustados quando contrato, schema ou fluxo mudar.
