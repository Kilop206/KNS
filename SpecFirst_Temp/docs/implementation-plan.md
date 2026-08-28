# Plano de Implementação — KNS

## 1. Objetivo

Organizar a evolução do KNS em fases verificáveis, evitando que o desenvolvimento avance sem critérios claros.

O plano deve refletir o estado real do projeto e ser atualizado conforme as issues forem concluídas.

---

# 2. Estado atual

O KNS já possui uma base funcional composta por:

* simulação orientada a eventos;
* tempo lógico;
* topologia;
* nós;
* Links;
* roteamento;
* transmissão de pacotes;
* GUI;
* headless mode;
* testes automatizados;
* TCP simplificado;
* handshake;
* ACK/data;
* encerramento;
* `TIME_WAIT`.

A evolução deve preservar essa base.

---

# 3. Fase 1 — Fundação SpecFirst

**Status:** Concluída

### Objetivo

Adaptar completamente o framework SpecFirst ao KNS.

### Checklist

* [x] Adaptar `AGENTS.md`.
* [x] Adaptar `README.md`.
* [x] Adaptar `docs/README.md`.
* [x] Adaptar `project-overview.md`.
* [x] Adaptar `architecture.md`.
* [x] Adaptar `ai-workflow.md`.
* [x] Adaptar `coding-standards.md`.
* [x] Adaptar `testing.md`.
* [x] Adaptar `domains.md`.
* [x] Adaptar `data-model.md`.
* [x] Adaptar `workflows.md`.
* [x] Adaptar `implementation-governance.md`.
* [x] Adaptar `implementation-plan.md`.
* [x] Adaptar `issues.md`.
* [x] Revisar `decision-log.md`.
* [x] Adaptar `deployment-log.md`.
* [x] Revisar `security.md`.
* [x] Revisar `tooling-adapters.md` e referências.
* [x] Fazer revisão cruzada de todos os documentos.
* [x] Eliminar placeholders restantes.
* [x] Confirmar referências internas válidas.

---

# 4. Fase 1 — Integridade da transmissão

**Status:** Próxima fase

### Objetivo

Garantir que o caminho de transmissão represente corretamente aceitação, rejeição, filas e capacidade.

### Issues prioritárias

* #91 — resultado de `sendPacket()`;
* #94 — FIFO real;
* #92 — identidade do Link;
* #93 — Links DOWN.

### Critérios

* falha de transmissão não pode ser reportada como sucesso;
* fila deve preservar FIFO;
* capacidade deve ser respeitada;
* Link DOWN não deve transportar;
* identidade do Link deve permanecer disponível quando exigida.

---

# 5. Fase 2 — Consistência da Topology

**Status:** Planejada

### Objetivo

Fortalecer contratos e comportamento da topologia.

### Issues relacionadas

* #95;
* #96;
* #97;
* #98;
* #82;
* #81.

### Critérios

* entradas inválidas devem ser rejeitadas corretamente;
* índices devem possuir bounds seguros;
* contratos const/non-const devem ser consistentes;
* alterações da topologia devem possuir comportamento definido;
* roteamento deve refletir o estado atual da rede.

---

# 6. Fase 3 — Consistência do TCP

**Status:** Planejada

### Objetivo

Fortalecer a máquina de estados e o estado agregado das sessões.

### Issues relacionadas

* #99;
* #100;
* #101.

### Critérios

* transições inválidas devem ser rejeitadas;
* falhas de envio não devem gerar transições falsas;
* estado de `TCPSession` deve representar corretamente seus endpoints;
* GUI não deve confundir conexão agendada com execução.

---

# 7. Fase 4 — TCP Reliability

**Status:** Planejada

### Ordem prevista

```text
Buffers
   ↓
Sliding Window
   ↓
RTO
   ↓
Retransmission
   ↓
Duplicate ACK
   ↓
Fast Retransmit
   ↓
Delayed ACK
   ↓
Congestion Control
```

Issues relacionadas incluem:

* #74;
* #73;
* #76;
* #77;
* #75.

Cada etapa deve ser implementada incrementalmente.

---

# 8. Fase 5 — Arquitetura de rede futura

**Status:** Planejada

Possíveis evoluções:

* Node/Interface;
* listener TCP;
* múltiplas conexões;
* roteamento dinâmico;
* falhas avançadas de Links;
* edição dinâmica da topologia.

Issues relacionadas:

* #79;
* #80;
* #81;
* #95.

---

# 9. Fase 6 — Hardening

**Status:** Planejada

### Objetivo

Preparar o KNS para evolução e uso experimental mais amplo.

Inclui:

* cobertura de testes;
* validações;
* determinismo;
* observabilidade;
* automação;
* benchmarks;
* documentação;
* estabilidade da GUI;
* consistência do build.

---

# 10. Regras de avanço

Não iniciar uma fase posterior enquanto:

* a fase atual possuir bloqueios;
* existirem critérios obrigatórios não atendidos;
* testes essenciais estiverem quebrados;
* a documentação necessária estiver inconsistente.

Uma exceção deve ser uma decisão humana explícita e registrada.

---

# 11. Critérios globais

Uma entrega deve:

* preservar determinismo;
* respeitar a arquitetura;
* possuir testes relevantes;
* passar os checks aplicáveis;
* manter o comportamento documentado;
* atualizar a issue correspondente;
* atualizar o log técnico;
* atualizar o plano quando houver avanço de fase.

---

# 12. Dependências principais

A evolução do KNS deve considerar especialmente:

```text
Topology
   ↓
Routing
   ↓
Link
   ↓
Packet transmission
   ↓
Events
   ↓
TCP
```

Mudanças em camadas inferiores podem afetar camadas superiores.

Por isso, a ordem acima serve como referência de impacto, não como autorização para implementar tudo simultaneamente.
