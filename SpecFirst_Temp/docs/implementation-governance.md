# Governança de Implementação — KNS

## 1. Objetivo

Este documento define as regras para controlar escopo, autonomia, fases e decisões durante a evolução do KNS.

O objetivo é impedir que uma issue localizada resulte, sem aprovação, em uma alteração arquitetural ampla.

---

## 2. Princípios

O trabalho deve seguir:

```text
Especificação
    ↓
Escopo aprovado
    ↓
Menor incremento seguro
    ↓
Implementação
    ↓
Validação
    ↓
Documentação
```

As regras de governança devem permanecer subordinadas ao contrato de `AGENTS.md`.

---

## 3. Menor incremento seguro

Cada tarefa deve alterar somente o que for necessário para atingir seu objetivo.

Exemplo:

```text
Bug em transmissão
    ↓
corrigir contrato de transmissão
    ↓
adicionar teste
```

não:

```text
Bug em transmissão
    ↓
reescrever arquitetura de rede
    ↓
alterar TCP
    ↓
alterar GUI
    ↓
introduzir novas abstrações
```

sem justificativa e aprovação.

---

## 4. Bugfix, feature e refatoração

Sempre que possível, separar:

* **Bugfix:** corrige comportamento incorreto.
* **Feature:** adiciona comportamento novo.
* **Refatoração:** altera estrutura sem mudar comportamento esperado.
* **Docs:** altera documentação sem mudar comportamento.

Uma tarefa pode conter mais de uma categoria somente quando isso for necessário e estiver claramente justificado.

---

## 5. Travamento de escopo

A IA não deve:

* criar uma issue nova quando a atual ainda resolve o problema;
* expandir uma issue para uma feature não planejada;
* alterar arquitetura sem necessidade;
* trocar o contrato de uma API apenas por conveniência;
* trocar o toolchain do projeto;
* remover documentação sem aprovação;
* pular uma fase com pendências obrigatórias;
* transformar um bugfix em refatoração ampla.

Quando uma mudança adicional for necessária:

1. identificar a dependência;
2. explicar o impacto;
3. separar a mudança quando possível;
4. obter decisão humana quando houver alteração relevante de escopo ou arquitetura.

---

## 6. Autonomia da IA

A IA pode executar diretamente:

* correções locais;
* criação de testes;
* atualizações de documentação;
* refatorações pequenas dentro do escopo aprovado;
* mudanças mecânicas e reversíveis.

A IA deve solicitar validação antes de:

* mudar arquitetura;
* criar um novo domínio;
* alterar protocolo;
* mudar modelo de dados compartilhado;
* introduzir dependência relevante;
* mudar toolchain;
* alterar significativamente o comportamento da GUI;
* alterar critérios de aceitação;
* remover documentos.

---

## 7. Issues

Uma issue deve ser diagnosticada antes de implementação.

Classificações:

```text
Resolvida
Parcialmente resolvida
Ainda válida
Obsoleta
```

Uma issue parcialmente resolvida deve gerar somente as mudanças ainda necessárias.

---

## 8. Fases

Uma fase representa um conjunto lógico de entregas.

Uma fase só pode ser concluída quando:

* os itens obrigatórios estiverem concluídos;
* os testes e checks aplicáveis passarem;
* as issues relacionadas estiverem atualizadas;
* a documentação estiver sincronizada;
* riscos residuais estiverem registrados.

---

## 9. Bloqueios

Uma tarefa deve ser marcada como bloqueada quando:

* existe conflito de especificação;
* existe decisão humana pendente;
* a solução depende de outra tarefa ainda não concluída;
* um check obrigatório não pode ser executado;
* existe risco técnico que exige decisão.

O bloqueio deve ser documentado.

---

## 10. Decisões

Registrar em `docs/decision-log.md` decisões duradouras sobre:

* arquitetura;
* protocolo;
* contratos;
* modelos de dados;
* toolchain;
* segurança;
* fluxo operacional;
* estratégia de testes.

Não usar o Decision Log como histórico de cada alteração de código.

---

## 11. Rastreabilidade

Toda entrega relevante deve permitir responder:

```text
Por que mudou?
    ↓
Qual issue?
    ↓
Qual código?
    ↓
Qual teste?
    ↓
Qual documentação?
```

A relação deve permanecer recuperável através do issue tracker, plano e logs.

---

## 12. Critério de avanço

Não avançar uma fase apenas porque parte do código funciona.

O avanço deve considerar:

```text
Comportamento
+
Testes
+
Checks
+
Documentação
+
Escopo
```

---

## 13. Regra especial para o KNS

Por ser um simulador orientado a eventos, mudanças aparentemente pequenas podem afetar vários componentes.

Antes de alterar:

* `SimulationEngine`;
* `EventQueue`;
* `Topology`;
* `Link`;
* `Routing`;
* `Packet`;
* `TCPSession`;
* `TCPConnection`;

a IA deve procurar consumidores e testes existentes.

A existência de dependências não autoriza automaticamente uma refatoração ampla; ela serve para medir o impacto da mudança.
