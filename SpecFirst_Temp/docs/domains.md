# Domains

## Por que este arquivo existe

Este documento define fronteiras de responsabilidade. Ele evita que conceitos diferentes se misturem e ajuda humanos e agentes a decidir onde uma mudanca pertence.

## Natureza de Template

Este arquivo e um template. No primeiro chat de escopo, revise e ajuste dominios, responsabilidades, inclusoes, exclusoes e matriz de dependencia ao projeto real.

## Regras Globais

- Cada dominio deve ter responsabilidade clara.
- Dados compartilhados devem ter contratos claros.
- Validacao deve ocorrer na fronteira apropriada.
- Um novo dominio so deve existir se reduzir acoplamento ou esclarecer responsabilidade.

## Dominio: `[NOME_DO_DOMINIO]`

Responsavel por:

- `[responsabilidade]`

Inclui:

- `[item incluido]`

Nao inclui:

- `[item fora deste dominio]`

Repita a secao para cada dominio.

## Matriz de Dependencia

Declare quais dominios podem se conhecer.

- `[DOMINIO_A]` pode referenciar `[DOMINIO_B]`.
- `[DOMINIO_C]` nao deve depender de `[DOMINIO_D]`.
