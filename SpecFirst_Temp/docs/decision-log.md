# Decision Log — KNS

## 1. Objetivo

Este documento registra decisões duradouras que afetam arquitetura, protocolo, modelo, segurança, operação ou processo de desenvolvimento do KNS.

Não é um histórico geral de commits.

---

# 2. Estados

Uma decisão pode estar:

* **Proposta**
* **Aceita**
* **Substituída**
* **Rejeitada**

Decisões substituídas devem apontar para a decisão que as substituiu.

---

# 3. Decisões do KNS

## 0001 — Manter o core independente da GUI

* **Data:** anterior ao processo SpecFirst
* **Estado:** Aceita

### Contexto

O simulador possui aplicação gráfica e execução headless.

### Decisão

A lógica principal do simulador deve permanecer no `core`, sem dependência da GUI.

### Consequências

* `core` pode ser utilizado em headless;
* GUI atua como camada de aplicação;
* testes podem executar o núcleo sem OpenGL/ImGui.

---

## 0002 — Adotar simulação determinística orientada a eventos

* **Data:** anterior ao processo SpecFirst
* **Estado:** Aceita

### Contexto

Experimentos de rede precisam ser reproduzíveis.

### Decisão

O KNS utiliza tempo lógico e ordenação determinística de eventos.

### Consequências

* execuções podem ser reproduzidas;
* testes podem verificar ordem de eventos;
* comportamento não deve depender do relógio da GUI.

---

## 0003 — Usar Dijkstra para o roteamento atual

* **Data:** anterior ao processo SpecFirst
* **Estado:** Aceita

### Contexto

O simulador precisa encontrar caminhos em topologias arbitrárias.

### Decisão

O roteamento atual utiliza Dijkstra para construir as tabelas necessárias à transmissão.

### Consequências

* cálculo de rota permanece separado da transmissão;
* mudanças dinâmicas da topologia podem exigir evolução futura do mecanismo.

---

## 0004 — Utilizar C++20 e CMake

* **Data:** anterior ao processo SpecFirst
* **Estado:** Aceita

### Decisão

O KNS utiliza C++20 e CMake como base de desenvolvimento.

### Consequências

* código deve permanecer compatível com C++20;
* build deve ser reproduzível;
* dependências são gerenciadas através do sistema de build.

---

## 0005 — Manter TCP como modelo simplificado

* **Data:** anterior ao processo SpecFirst
* **Estado:** Aceita

### Decisão

O TCP do KNS representa um subconjunto determinístico e orientado à simulação, não uma implementação completa da pilha TCP do sistema operacional.

### Consequências

Novos mecanismos devem ser adicionados incrementalmente sobre a arquitetura existente.

---

## 0006 — Separar criação de conexão e execução da simulação

* **Data:** 2026-08-28
* **Estado:** Aceita

### Contexto

A interação da GUI pode criar/agendar uma conexão TCP antes do início da execução.

### Decisão

Criar ou agendar uma conexão não deve iniciar automaticamente o processamento da simulação.

### Consequências

O ciclo da simulação permanece explicitamente:

```text
READY
 ↓
RUNNING
 ↓
PAUSED
 ↓
RUNNING
 ↓
FINISHED
```

A conexão pode existir enquanto a simulação permanece pausada.

---

# 4. Decisões do SpecFirst

As decisões históricas relacionadas ao próprio framework permanecem válidas como decisões de governança do processo.

Elas não devem ser confundidas com decisões do domínio do KNS.

Entre elas:

* `AGENTS.md` como contrato universal;
* sincronização entre issue, plano e log técnico;
* humano como navegador e IA como piloto;
* menor incremento seguro;
* aprovação humana para mudanças relevantes.

---

# 5. Regra

Uma nova decisão só deve ser registrada quando tiver efeito duradouro.

Não registrar:

* pequenos detalhes de implementação;
* cada commit;
* ajustes triviais;
* correções locais sem consequência arquitetural.
