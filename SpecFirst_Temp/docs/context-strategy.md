# Context Strategy

## Por que este arquivo existe

Este documento define como fornecer contexto para agentes de IA sem sobrecarregar a conversa. Ele ajuda a decidir o que deve ficar em `AGENTS.md`, o que deve ficar em docs e o que deve ser lido sob demanda.

## Natureza de Template

Este arquivo e um template. No primeiro chat de escopo, revise e ajuste camadas de contexto, regras de leitura e fronteiras entre memoria da conversa, docs e codigo ao projeto real.

## Camadas de Contexto

1. `AGENTS.md`: regras obrigatorias e indice de contexto.
2. Docs nucleares: arquitetura, workflow, standards e testes.
3. Docs sob demanda: dominio, seguranca, dados, operacao e decisoes.
4. Codigo local: fonte final para comportamento implementado.

## Regras

- Nao duplicar o mesmo contrato em varios arquivos.
- Preferir links entre docs em vez de copiar secoes inteiras.
- Manter documentos curtos o bastante para leitura frequente.
- Registrar decisoes duradouras em `docs/decision-log.md`.
