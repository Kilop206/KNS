# Issues — KNS

## 1. Objetivo

Este documento mantém o estado operacional das issues relevantes para o desenvolvimento do KNS.

O GitHub continua sendo o issue tracker oficial.

Este arquivo existe para manter uma visão documental sincronizada com o plano e o histórico técnico.

---

# 2. Estados

Uma issue pode estar:

* **Planejada**
* **Em andamento**
* **Concluída**
* **Bloqueada**
* **Resolvida**
* **Parcialmente resolvida**
* **Obsoleta**

A classificação de uma issue deve refletir o código e a especificação atuais.

---

# 3. Regra de diagnóstico

Antes de implementar:

```text
Issue
 ↓
Código atual
 ↓
Documentação
 ↓
Testes
 ↓
Classificação
```

Não implementar uma issue apenas porque ela permanece aberta.

---

# 4. Issues prioritárias atuais

## ISSUE-091 — `sendPacket()` pode reportar sucesso incorretamente

**Tipo:** Bug
**Status:** Planejada
**Fase:** Fase 1 — Integridade da transmissão

### Objetivo

Garantir que o resultado da transmissão seja propagado corretamente pelo caminho de envio.

### Critérios de aceite

* envio aceito deve ser reportado como sucesso;
* envio rejeitado deve ser reportado como falha;
* `sendPacketThroughTopology()` não pode retornar sucesso após uma rejeição;
* testes de regressão devem cobrir ambos os caminhos.

### Docs relevantes

* `docs/architecture.md`
* `docs/data-model.md`
* `docs/workflows.md`
* `docs/testing.md`

### Estado atual

* **2026-08-28:** Identificada divergência entre o resultado real da transmissão e o resultado reportado ao chamador.

---

## ISSUE-094 — Fila FIFO real no Link

**Tipo:** Bug / Design
**Status:** Planejada
**Fase:** Fase 1 — Integridade da transmissão

### Objetivo

Garantir que a fila de pacotes de um Link preserve a ordem de chegada e respeite sua capacidade.

### Critérios de aceite

* primeiro pacote inserido deve ser o primeiro removido;
* fila cheia deve rejeitar novos pacotes conforme o contrato;
* tamanho da fila deve permanecer consistente;
* caminho real de transmissão deve utilizar a fila quando o contrato exigir;
* testes devem verificar múltiplos pacotes em sequência.

### Docs relevantes

* `docs/architecture.md`
* `docs/data-model.md`
* `docs/testing.md`
* `docs/workflows.md`

### Estado atual

* **2026-08-28:** Estrutura de fila já existe em `Link`, mas precisa ser validada contra o caminho real de transmissão.

---

# 5. Dependências

As issues #91 e #94 possuem forte relação:

```text
#94
fila/capacidade
   ↓
resultado da transmissão
   ↓
#91
```

Alterações devem evitar duplicação de lógica entre os dois problemas.

---

# 6. Issues relacionadas

* #92 — identidade do Link em trânsito;
* #93 — respeito a Links DOWN;
* #95 — eventos após mudanças da topologia;
* #96 — validações adicionais da Topology;
* #97 — bounds de `getNextHop()`;
* #98 — contrato de `getLinksFromNode()`;
* #99 — estado agregado de `TCPSession`;
* #100 — falhas nas transições de `TCPConnection`;
* #101 — lifecycle da simulação.

---

# 7. Regra de sincronização

Ao concluir uma issue:

1. atualizar seu status neste arquivo;
2. registrar progresso no GitHub;
3. atualizar `implementation-plan.md`;
4. registrar a entrega em `deployment-log.md`;
5. atualizar documentos afetados;
6. registrar decisão em `decision-log.md` quando aplicável.

---

# 8. Histórico

Novas entradas devem ser adicionadas em `### Estado atual` de cada issue, com data e resumo objetivo.

Não apagar histórico relevante para substituir por uma nova descrição.
