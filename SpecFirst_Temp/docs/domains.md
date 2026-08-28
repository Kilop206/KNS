# Domínios e Fronteiras — KNS

## 1. Objetivo

Este documento define as principais fronteiras de responsabilidade do KNS.

Seu objetivo é evitar que lógica de simulação, rede, transporte e apresentação sejam misturadas sem necessidade.

A pergunta principal deste documento é:

> **"Onde esta responsabilidade deve viver?"**

---

## 2. Princípios

Cada domínio deve possuir uma responsabilidade clara.

Uma mudança deve ser implementada no menor domínio capaz de representar corretamente o comportamento.

Uma camada não deve assumir responsabilidades pertencentes a outra apenas porque possui acesso aos dados necessários.

Quando uma responsabilidade atravessar fronteiras, o contrato entre os domínios deve ser explícito.

---

# 3. Domínio: Simulação

**Responsabilidade:** controlar o tempo lógico e a execução dos eventos.

### Inclui

* `SimulationClock`;
* `Event`;
* `EventQueue`;
* `SimulationEngine`;
* processamento de eventos;
* agendamento;
* avanço do tempo;
* execução headless;
* coordenação das estatísticas.

### Não inclui

* regras específicas da GUI;
* renderização;
* lógica visual de pacotes;
* detalhes gráficos;
* implementação de controles ImGui.

---

# 4. Domínio: Rede

**Responsabilidade:** representar a infraestrutura simulada e o transporte de pacotes entre nós.

### Inclui

* `Topology`;
* `Link`;
* nós;
* adjacência;
* estado UP/DOWN dos Links;
* bandwidth;
* delay;
* loss;
* modos de duplex;
* capacidade e filas dos Links;
* transmissão física/logística dos pacotes.

### Não inclui

* regras próprias da máquina TCP;
* renderização de pacotes;
* controles de interface;
* decisões específicas de apresentação.

---

# 5. Domínio: Roteamento

**Responsabilidade:** determinar como um pacote deve avançar pela topologia.

### Inclui

* tabelas de roteamento;
* seleção de próximo salto;
* cálculo de caminhos;
* Dijkstra;
* atualização das tabelas quando a arquitetura exigir.

### Não inclui

* transmissão física pelo Link;
* gerenciamento da fila do Link;
* regras TCP;
* visualização do caminho.

O roteamento decide **por onde** um pacote deve seguir.

O domínio de rede decide **como** o pacote é transportado.

---

# 6. Domínio: Pacotes

**Responsabilidade:** representar a unidade de comunicação que atravessa o simulador.

### Inclui

* `Packet`;
* `PacketType`;
* dados de origem/destino;
* posição atual;
* nó anterior;
* timestamps;
* tamanho;
* hop count;
* identificação de sessão;
* encapsulamento do segmento TCP.

### Não inclui

* cálculo do caminho;
* transmissão do Link;
* execução da GUI;
* controle da sessão TCP.

O pacote transporta informações; ele não deve assumir o papel do motor de simulação.

---

# 7. Domínio: Transporte / TCP

**Responsabilidade:** modelar o comportamento do protocolo de transporte TCP definido pelo KNS.

### Inclui

* `TCPSession`;
* `TCPConnection`;
* `TCPState`;
* `TCPStateMachine`;
* `TCPSegment`;
* sequence numbers;
* acknowledgement numbers;
* SYN;
* SYN-ACK;
* ACK;
* DATA;
* FIN;
* estados de conexão;
* encerramento;
* mecanismos TCP já implementados.

### Não inclui

* escolha direta de Links;
* implementação de Dijkstra;
* renderização;
* desenho da topologia;
* controle do relógio real da interface.

O TCP cria e interpreta segmentos.

A transmissão desses segmentos deve continuar passando pela infraestrutura de rede do KNS.

---

# 8. Domínio: Aplicação / GUI

**Responsabilidade:** apresentar e controlar a simulação.

### Inclui

* aplicação executável;
* ImGui;
* GLFW;
* OpenGL;
* janelas;
* controles;
* visualização da topologia;
* renderização de pacotes;
* gráficos;
* logs;
* interação com nós.

### Não inclui

* implementação duplicada de roteamento;
* máquina TCP;
* cálculo físico de transmissão;
* regras internas de filas;
* estado alternativo do `SimulationEngine`.

A GUI deve atuar como cliente do `core`.

---

# 9. Domínio: Configuração e Topologias

**Responsabilidade:** transformar configurações externas em estruturas utilizáveis pela simulação.

### Inclui

* arquivos JSON de topologia;
* `TopologyLoader`;
* parâmetros de nós e Links;
* validação das entradas;
* configuração inicial da simulação.

### Não inclui

* lógica de execução dos eventos;
* decisões do protocolo TCP;
* renderização.

---

# 10. Domínio: Estatísticas e Observabilidade

**Responsabilidade:** registrar e expor informações sobre a execução da simulação.

### Inclui

* `Stats`;
* contadores;
* latência;
* perda;
* entrega;
* throughput;
* exportação;
* observadores definidos pelo engine;
* logs e informações necessárias para análise.

### Não inclui

* alterar o comportamento da simulação apenas para produzir uma métrica;
* implementar regras TCP;
* escolher caminhos de roteamento.

---

# 11. Domínio: Testes

**Responsabilidade:** verificar os contratos dos demais domínios.

### Inclui

* testes unitários;
* testes de integração;
* testes de regressão;
* cenários determinísticos;
* validação das fronteiras entre domínios.

### Não inclui

* copiar a implementação apenas para fazê-la passar;
* substituir a especificação;
* esconder uma falha alterando expectativas sem justificativa.

---

# 12. Matriz de dependências

A direção preferencial é:

```text
GUI / app
    ↓
SimulationEngine
    ↓
Network / Routing / Packets / TCP
```

e:

```text
tests
   ↓
core
```

O `core` não deve depender de `app`.

### Dependências permitidas

* `app` → `core`;
* `tests` → `core`;
* `SimulationEngine` → componentes de rede e transporte;
* `PacketReceivedEvent` → pacote/TCP conforme o contrato do evento;
* TCP → abstrações fornecidas pelo core de rede/simulação.

### Dependências proibidas

* `core` → GUI;
* TCP → ImGui/GLFW/OpenGL;
* `Routing` → renderização;
* `Link` → janelas da GUI;
* `Packet` → estado visual;
* `Stats` → lógica de apresentação.

---

# 13. Regra para novas responsabilidades

Antes de criar um novo domínio ou mover uma responsabilidade:

1. verificar se já existe domínio apropriado;
2. verificar se a nova fronteira reduz acoplamento;
3. evitar abstração para um único uso;
4. registrar decisão arquitetural quando a mudança for significativa.

A criação de um domínio deve esclarecer a arquitetura, não apenas aumentar o número de classes.
