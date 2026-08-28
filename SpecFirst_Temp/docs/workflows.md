# Workflows — KNS

## 1. Objetivo

Este documento descreve os principais fluxos operacionais do KNS.

Detalhes internos de implementação devem permanecer nos documentos técnicos correspondentes.

---

# 2. Fluxo principal

```text
Carregar topologia
       ↓
Validar topologia
       ↓
Criar SimulationEngine
       ↓
Construir/reconstruir roteamento
       ↓
Configurar simulação
       ↓
Criar/agendar conexões ou tráfego
       ↓
READY / PAUSED
       ↓
START
       ↓
RUNNING
       ↓
processar eventos
       ↓
PAUSE / RESUME / STEP
       ↓
fila de eventos termina
       ↓
FINISHED
```

---

# 3. Carregamento de topologia

### Entrada

Um arquivo JSON de topologia fornecido:

* pela linha de comando;
* pela GUI;
* por uma rotina de teste.

### Fluxo

1. localizar o arquivo;
2. ler o JSON;
3. validar a estrutura;
4. criar a `Topology`;
5. criar os Links;
6. configurar propriedades;
7. construir o `SimulationEngine`;
8. preparar as estruturas de roteamento.

### Saída

Uma topologia válida pronta para ser simulada.

### Falhas possíveis

* arquivo inexistente;
* JSON inválido;
* número inválido de nós;
* Link inválido;
* endpoints inexistentes;
* propriedades inválidas.

Falhas devem ser reportadas sem produzir uma topologia parcialmente válida.

---

# 4. Inicialização da simulação

Depois que a topologia é carregada:

### Sem eventos

```text
READY
```

### Com eventos previamente agendados

```text
PAUSED
```

O estado não deve ser interpretado apenas pela ausência ou presença de eventos sem considerar o ciclo de vida da simulação.

---

# 5. Início da simulação

A ação `Start` inicia o processamento.

```text
READY
   ↓
Start
   ↓
RUNNING
```

Quando uma conexão TCP é criada/agendada pela interação da rede, isso não significa que a simulação começou.

Criar uma conexão e executar a simulação são operações distintas.

---

# 6. Pause

Durante:

```text
RUNNING
```

o usuário pode pausar.

```text
RUNNING
   ↓
Pause
   ↓
PAUSED
```

O relógio visual e o processamento de eventos deixam de avançar pela execução normal.

Eventos ainda podem permanecer agendados.

---

# 7. Resume

Em:

```text
PAUSED
```

o usuário pode continuar:

```text
PAUSED
   ↓
Resume
   ↓
RUNNING
```

O próximo evento deve continuar a partir do estado lógico atual.

---

# 8. Step

Em:

```text
PAUSED
```

o usuário pode executar um evento individual.

```text
PAUSED
   ↓
Step
   ↓
processar próximo evento
   ↓
PAUSED
```

Caso o processamento consuma o último evento:

```text
PAUSED
   ↓
Step
   ↓
sem eventos restantes
   ↓
FINISHED
```

---

# 9. Finalização

Quando o último evento válido da simulação for processado:

```text
RUNNING
     ↓
event queue vazia
     ↓
FINISHED
```

A ausência de eventos antes da execução não deve ser tratada automaticamente como término.

---

# 10. Criação de conexão TCP

Quando o usuário inicia uma conexão através da interface:

```text
selecionar origem
      ↓
arrastar para destino
      ↓
criar/agendar conexão
```

Essa ação deve configurar o trabalho futuro do TCP.

Ela não deve executar automaticamente:

```text
SYN
↓
SYN-ACK
↓
ACK
↓
DATA
```

Esses eventos dependem da execução da simulação.

---

# 11. Handshake TCP

Fluxo lógico:

```text
cliente
  │
  │ SYN
  ▼
rede
  │
  ▼
servidor
  │
  │ SYN-ACK
  ▼
rede
  │
  ▼
cliente
  │
  │ ACK
  ▼
rede
  │
  ▼
servidor
```

Estados principais:

```text
Cliente:
CLOSED → SYN_SENT → ESTABLISHED

Servidor:
CLOSED → SYN_RECEIVED → ESTABLISHED
```

A especificação detalhada permanece em:

```text
docs/protocol_spec.md
docs/tcp_design.md
```

---

# 12. Transmissão de pacote

Fluxo conceitual:

```text
evento/protocolo
      ↓
criação do Packet
      ↓
seleção do próximo salto
      ↓
seleção do Link
      ↓
verificação da possibilidade de transmissão
      ↓
fila/transmissão
      ↓
cálculo do tempo de chegada
      ↓
PacketReceivedEvent
```

Cada etapa deve respeitar o contrato do domínio correspondente.

---

# 13. Roteamento

Quando um pacote precisa avançar:

```text
Packet
  ↓
SimulationEngine / Routing
  ↓
next hop
  ↓
Link correspondente
```

O roteamento determina o próximo salto.

A transmissão determina quando e como o pacote percorre o Link.

Essas responsabilidades não devem ser combinadas indiscriminadamente.

---

# 14. Falha de transmissão

Uma transmissão pode falhar por motivos como:

* rota inexistente;
* Link inexistente;
* Link DOWN;
* fila cheia;
* condição de transmissão inválida;
* outras falhas especificadas pelo domínio.

O fluxo deve preservar o resultado:

```text
aceita
```

ou:

```text
rejeita
```

Uma falha não pode ser silenciosamente reinterpretada como sucesso pelo chamador.

---

# 15. Alteração da topologia

Quando a topologia for modificada:

```text
criar/remover nó
ou
criar/remover Link
ou
alterar estado do Link
```

o sistema deve verificar quais estruturas dependentes precisam ser atualizadas.

Especialmente:

* adjacência;
* tabelas de roteamento;
* pacotes/eventos já agendados;
* estado das transmissões;
* visualização.

O comportamento de eventos já agendados durante mudanças da topologia é um contrato específico que deve ser definido antes de uma implementação que o altere.

---

# 16. Execução headless

Fluxo:

```text
CLI
 ↓
parse argumentos
 ↓
load topology
 ↓
validate
 ↓
create engine
 ↓
configure
 ↓
schedule/run
 ↓
collect stats
 ↓
export results
```

A execução headless não deve depender da GUI.

---

# 17. Fluxo de desenvolvimento de uma issue

O workflow SpecFirst do KNS é:

```text
Issue
 ↓
análise do estado atual
 ↓
especificação
 ↓
critério de aceite
 ↓
teste/regressão
 ↓
implementação
 ↓
validação
 ↓
documentação
```

Uma issue aberta não é automaticamente uma especificação válida do código atual.

---

# 18. Fluxo de manutenção documental

Quando uma alteração modificar:

* comportamento;
* arquitetura;
* contrato;
* fluxo;
* estrutura;

a documentação correspondente deve ser atualizada.

O fluxo é:

```text
mudança
 ↓
identificar docs afetados
 ↓
atualizar
 ↓
verificar referências cruzadas
 ↓
registrar decisão, se necessário
```

---

# 19. Fluxo de entrega

Uma tarefa concluída exige:

```text
Código
 +
Testes
 +
Checks
 +
Issue
 +
Plano
 +
Deployment Log
 +
Docs afetados
```

Somente depois disso a tarefa pode ser considerada concluída.
