# Security

## Por que este arquivo existe

Este documento registra controles minimos de seguranca. Ele deve orientar mudancas em autenticacao, autorizacao, uploads, secrets, dados sensiveis e exposicao publica.

## Natureza de Template

Este arquivo e um template. No primeiro chat de escopo, revise e ajuste modelo de acesso, auth, autorizacao, validacao de input, secrets, dados publicos/privados e logs seguros ao projeto real.

## Modelo de Acesso

Descreva quem acessa o sistema e com quais papeis.

- `[papel]`: `[permissoes]`

## Autenticacao e Autorizacao

- `[como usuarios autenticam]`
- `[onde permissoes sao verificadas]`
- `[acoes sensiveis] devem ser validadas no backend`

## Validacao de Input

- Entradas externas devem ser validadas com `[ferramenta ou camada]`.
- Mensagens de erro devem ser claras sem revelar detalhes internos.

## Secrets e Configuracao

- Segredos devem ficar em variaveis de ambiente.
- `.env.example` deve documentar variaveis obrigatorias sem valores reais.
- Nenhum segredo deve ser exposto no frontend, logs ou exemplos.

## Dados Publicos e Privados

- Declare quais dados podem ser serializados para o cliente.
- Declare quais dados nunca devem sair do backend.

## Observabilidade Segura

- Logs devem ajudar investigacao sem registrar senhas, tokens ou payloads sensiveis.
