# Deployment Log — KNS

## 1. Objetivo

Este documento registra as entregas técnicas efetivamente realizadas no KNS.

Ele responde:

> O que foi entregue, por qual issue, em qual fase e com quais validações?

---

# 2. Regras

Toda entrega relevante deve registrar:

* data;
* issue ou contexto;
* fase;
* comportamento entregue;
* arquivos modificados;
* testes/checks;
* documentação afetada;
* riscos residuais.

As entradas mais recentes ficam no topo.

---

# 3. Diferença entre registros

### `issues.md`

Mostra o estado vivo do trabalho.

### `implementation-plan.md`

Mostra onde a implementação está e para onde vai.

### `decision-log.md`

Explica decisões duradouras.

### `deployment-log.md`

Registra o que foi tecnicamente entregue.

---

# 4. Entregas relacionadas à adaptação SpecFirst

## [2026-08-28] — Adaptação inicial do contrato ao KNS

* **Fase:** Fase 0 — Fundação SpecFirst
* **Contexto:** adaptação documental
* **O que foi feito:** redefinição do contrato operacional do SpecFirst para refletir o KNS, incluindo C++20, CMake, core/app/tests, simulação determinística, rede, TCP, GUI, headless e regras de trabalho com issues.
* **Arquivos:** `SpecFirst_Temp/AGENTS.md`
* **Checks:** revisão documental contra a estrutura atual da branch `tcp`.
* **Riscos:** demais documentos do framework ainda estavam em processo de adaptação.

---

## [2026-08-28] — Primeiro lote documental

* **Fase:** Fase 0 — Fundação SpecFirst
* **Contexto:** adaptação documental
* **O que foi feito:** adaptação de README, índice de docs, visão geral, arquitetura, workflow de IA, coding standards e testing.
* **Arquivos:** documentos correspondentes em `SpecFirst_Temp/`.
* **Checks:** revisão cruzada com estrutura atual do KNS.
* **Riscos:** documentos especializados ainda precisavam ser adaptados.

---

## [2026-08-28] — Segundo lote documental

* **Fase:** Fase 0 — Fundação SpecFirst
* **Contexto:** adaptação documental
* **O que foi feito:** adaptação de domínios, modelo de dados e workflows.
* **Checks:** comparação com `Topology`, `Packet`, estrutura do core e fluxos atuais.
* **Riscos:** governança e rastreabilidade ainda precisavam ser adaptadas.

---

# 5. Entregas de engenharia

Novas entregas técnicas devem ser adicionadas nesta seção.

Formato:

```md
## [AAAA-MM-DD] — ISSUE-XXX

- **Fase:** [fase]
- **O que foi feito:** [entrega]
- **Arquivos modificados:** `[arquivo]`
- **Resultados dos testes:** [resultado]
- **Docs atualizados:** `[arquivo]`
- **Riscos/Débito técnico:** [risco ou "Nenhum conhecido"]
```

---

# 6. Nota sobre histórico anterior

As entradas históricas originalmente presentes neste arquivo descreviam principalmente a evolução do framework SpecFirst, e não entregas do KNS.

Elas não devem ser interpretadas como histórico de implementação do simulador.

O histórico original deve ser preservado em caso de necessidade de auditoria, mas o registro operacional futuro deve utilizar este formato específico do KNS.
