# Event Engine Design

## 1. Purpose

The Event Engine is the core of KNS.  
It implements a discrete-event simulation model focused on determinism and reproducibility.

---

## 2. Simulation Time Model

- Time type: int64_t
- Unit: logical ticks
- Time is logical and does not depend on the system clock.
- Time advances only when an event is processed.

**Rationale:**

Using int64_t ensures a large time range suitable for long-running simulations.  
The logical time model provides full control over time progression, eliminating external dependencies and ensuring determinism.

---

## 3. Event Definition

An event is defined as:

- Timestamp (logical time)
- Unique incremental identifier
- Action to be executed

**Responsibility:**

An event represents a scheduled action to occur at a specific simulation time.

---

## 4. Event Ordering Policy

Events are ordered according to the following rules:

1. Lower timestamp first
2. In case of ties, lower ID first (insertion order)

This policy guarantees absolute determinism.

---

## 5. Execution Model

The engine operates as follows:

1. While the queue is not empty:
    - Remove the next event
    - Update the current simulation time
    - Execute the associated action

The engine can:

- Run until the queue is empty
- Run until a time limit is reached
- Be paused and resumed

---

## 6. Determinism Guarantee

Given the same initial set of events and the same insertion order:

- Execution will always produce the same event order
- The final state will always be identical

---

## 7. Known Limitations (Initial)

- Single-threaded execution
- No parallelism in the core
- No dependency on real time

---
---

# Event Engine Design

## 1. Propósito

O Event Engine é o núcleo do KNS.  
Ele implementa um modelo de simulação orientado a eventos discretos com foco em determinismo e reprodutibilidade.

---

## 2. Modelo de Tempo de Simulação

- Tipo de tempo: int64_t
- Unidade: ticks lógicos
- O tempo é lógico e não depende do relógio do sistema.
- O tempo avança apenas quando um evento é processado.

**Justificativa da escolha:**

Ao usar int64_t, garantimos uma ampla faixa de tempo para simulações longas.  
O modelo lógico permite controle total sobre o avanço do tempo, eliminando dependências externas e garantindo determinismo.

---

## 3. Definição de Evento

Um evento é definido como:

- Timestamp (tempo lógico)
- Identificador único incremental
- Ação a ser executada

**Responsabilidade:**

O evento representa uma ação agendada para ocorrer em um tempo específico da simulação.

---

## 4. Política de Ordenação de Eventos

A ordenação dos eventos segue as regras:

1. Menor timestamp primeiro
2. Em caso de empate, menor ID primeiro (ordem de inserção)

Essa política garante determinismo absoluto.

---

## 5. Modelo de Execução

O motor executa da seguinte forma:

1. Enquanto a fila não estiver vazia:
    - Remove o próximo evento
    - Atualiza o tempo atual da simulação
    - Executa a ação associada

O motor pode:

- Rodar até esvaziar a fila
- Rodar até atingir um tempo limite
- Ser pausado e retomado

---

## 6. Garantia de Determinismo

Dado o mesmo conjunto inicial de eventos e a mesma ordem de inserção:

- A execução produzirá sempre a mesma ordem de eventos
- O estado final será sempre idêntico

---

## 7. Limitações Conhecidas (Inicial)

- Execução single-threaded
- Não utiliza paralelismo no núcleo
- Não depende de tempo real