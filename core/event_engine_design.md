# Event Engine Design

## 1. Purpose

O Event Engine é o núcleo do GNS.  
Ele implementa um modelo de simulação orientado a eventos discretos com foco em determinismo e reprodutibilidade.

---

## 2. Simulation Time Model

- Tipo de tempo: int64_t
- Unidade: ticks lógicos
- O tempo é lógico e não depende do relógio do sistema.
- O tempo avança apenas quando um evento é processado.

Justificativa da escolha:
Ao usar int64_t, garantimos uma ampla faixa de tempo para simulações longas. O modelo lógico permite controle total sobre o avanço do tempo, eliminando dependências externas e garantindo determinismo.

## 3. Event Definition

Um evento é definido como:

- Timestamp (tempo lógico)
- Identificador único incremental
- Ação a ser executada

Responsabilidade:
O evento representa uma ação agendada para ocorrer em um tempo específico da simulação.

---

## 4. Event Ordering Policy

A ordenação dos eventos segue as regras:

1. Menor timestamp primeiro
2. Em caso de empate, menor ID primeiro (ordem de inserção)

Essa política garante determinismo absoluto.

---

## 5. Execution Model

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

## 6. Determinism Guarantee

Dado o mesmo conjunto inicial de eventos e a mesma ordem de inserção:

- A execução produzirá sempre a mesma ordem de eventos
- O estado final será sempre idêntico

---

## 7. Known Limitations (Inicial)

- Execução single-threaded
- Não utiliza paralelismo no núcleo
- Não depende de tempo real