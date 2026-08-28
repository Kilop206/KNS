# Modelo de Dados e Estruturas — KNS

## 1. Objetivo

Este documento descreve as principais estruturas de dados usadas pelo KNS e os contratos que elas representam.

O KNS não utiliza um banco de dados como parte do núcleo da simulação. O "modelo de dados" aqui significa as estruturas em memória, formatos de configuração e contratos de dados entre componentes.

---

# 2. Princípios

* Estruturas devem representar conceitos reais do simulador.
* Campos devem possuir significado claro.
* Unidades devem ser explícitas quando relevantes.
* Identificadores devem possuir semântica consistente.
* Dados de domínio não devem depender da GUI.
* Alterações estruturais devem considerar impacto nos consumidores.
* Dados utilizados para determinar comportamento devem permanecer determinísticos.

---

# 3. Topology

`Topology` representa a rede simulada.

### Responsabilidade

Manter:

* quantidade de nós;
* Links;
* adjacência;
* nome da topologia;
* operações de criação/removação;
* estado dos Links.

A implementação atual utiliza:

```text
LinkPtr = std::shared_ptr<Link>
```

e mantém Links e listas de adjacência internamente.

### Conceito

```text
Topology
├── nodes
├── links
├── adjacency
└── name
```

### Regras

* IDs de nós devem ser válidos dentro dos limites da topologia.
* Links devem referenciar endpoints existentes.
* A lista de adjacência deve permanecer consistente com os Links.
* Remoção de nós/Links não deve deixar referências inválidas.
* Alterações estruturais devem considerar tabelas de roteamento já existentes.

---

# 4. Link

`Link` representa uma conexão entre dois nós.

### Dados principais

```text
a
b
bandwidth_mbps
delay_ms
loss_prob
mode
up/down
queue state
transmission state
```

### Modos

O KNS atualmente possui:

```text
FULL_DUPLEX
HALF_DUPLEX
SIMPLEX
```

### Regras

* `a` e `b` identificam os endpoints.
* Um Link não deve ser usado quando estiver `DOWN`.
* Bandwidth deve ser interpretado em Mbps.
* Delay deve ser interpretado em ms.
* Loss probability deve representar a probabilidade configurada para o Link.
* A capacidade da fila deve ser respeitada.
* Quando o contrato exigir fila, a ordem deve ser FIFO.
* O estado interno de transmissão deve ser consistente com o modo duplex.

---

# 5. Packet

`Packet` representa a unidade transportada pelo simulador.

A estrutura atual contém:

```text
source
destination
current_node
previous_node
creation_time
departure_time
packet_size_bytes
hop_count
session_id
tcp
packet_type
```

### Regras

`source`:

* nó de origem lógica do pacote.

`destination`:

* nó de destino lógico.

`current_node`:

* nó onde o pacote atualmente se encontra no modelo da simulação.

`previous_node`:

* nó do qual o pacote chegou ao estado atual, quando disponível.

`creation_time`:

* instante lógico de criação.

`departure_time`:

* instante lógico associado à última partida relevante.

`packet_size_bytes`:

* tamanho do pacote em bytes.

`hop_count`:

* quantidade de hops realizados.

`session_id`:

* identificação da sessão TCP associada, quando aplicável.

`tcp`:

* segmento TCP encapsulado.

`packet_type`:

* classificação de alto nível do pacote.

---

# 6. PacketTravelInfo

`PacketTravelInfo` representa informações de um pacote durante seu trânsito visual/lógico.

O modelo atual prevê:

```text
departure_time
arrival_time
from_node
to_node
packet_type
link_from
link_to
```

Os campos `link_from` e `link_to` existem para preservar a identificação do Link quando essa informação for necessária ao modelo de trânsito.

---

# 7. TCP Segment

`TCPSegment` representa a unidade de transporte TCP.

O modelo atual contempla:

* sequence number;
* acknowledgement number;
* advertised window;
* flags;
* payload.

As flags utilizadas pelo protocolo atual incluem:

```text
SYN
ACK
FIN
PSH
```

A combinação:

```text
ACK + PSH
```

é utilizada para representar DATA.

---

# 8. TCPSession

Uma `TCPSession` representa uma conexão simulada.

Conceitualmente:

```text
TCPSession
├── session_id
├── source
├── destination
├── session state
├── client TCPConnection
└── server TCPConnection
```

A sessão também mantém informações relacionadas à geração e acompanhamento do tráfego.

As duas pontas são conhecidas dentro do mesmo processo de simulação.

---

# 9. TCPConnection

`TCPConnection` representa o estado de um endpoint TCP.

Dados conceituais incluem:

* estado;
* sequência local;
* ACK esperado;
* nó local;
* nó remoto;
* informações de retry.

Sua responsabilidade é representar a evolução do endpoint TCP, não a transmissão física do pacote.

---

# 10. Event

Eventos representam ações associadas ao tempo lógico da simulação.

Um evento possui, conceitualmente:

```text
timestamp
+
tipo específico
+
dados necessários para executar
```

Os eventos são armazenados com ownership explícito no mecanismo de eventos.

---

# 11. EventQueue

A fila de eventos representa o futuro da simulação.

Sua ordem é definida pelo timestamp e pelo mecanismo de desempate determinístico adotado pelo projeto.

Ela não deve ser confundida com a fila de pacotes de um `Link`.

```text
EventQueue
    ≠
Link queue
```

São dois conceitos diferentes.

---

# 12. Routing Table

As tabelas de roteamento são mantidas pelo `SimulationEngine`.

O modelo atual utiliza entradas que permitem determinar o próximo salto.

Uma entrada deve possuir significado suficiente para:

* identificar destino;
* determinar próximo salto;
* representar a rota calculada;
* permitir validação de índices.

Rotas devem permanecer coerentes com a topologia utilizada na simulação.

---

# 13. Statistics

`Stats` representa dados agregados da execução.

Exemplos:

* packets sent;
* packets delivered;
* packets lost;
* latency;
* throughput;
* outras métricas produzidas pelo engine.

Estatísticas não devem ser utilizadas como fonte de verdade do estado operacional da rede.

Elas descrevem o comportamento; não devem governá-lo.

---

# 14. Configuração externa

Topologias são representadas por JSON.

Exemplo conceitual:

```json
{
  "nodes": 4,
  "name": "mesh4",
  "links": [
    {
      "from": 0,
      "to": 1,
      "delay": 5,
      "bandwidth": 100,
      "loss": 0.0,
      "mode": "full_duplex"
    }
  ]
}
```

Entradas externas devem ser validadas antes de serem incorporadas ao modelo interno.

---

# 15. Identificadores

IDs utilizados para nós e sessões devem ter semântica consistente.

Regras:

* IDs de nós são utilizados para indexar a topologia;
* `session_id` identifica uma sessão TCP;
* IDs não devem ser reinterpretados entre camadas;
* quando uma identidade precisar sobreviver ao trânsito de uma estrutura, ela deve ser transportada explicitamente.

---

# 16. Evolução do modelo

Alterações em estruturas centrais devem verificar:

```text
estrutura
 ↓
consumidores
 ↓
eventos
 ↓
testes
 ↓
GUI/observabilidade
```

Mudanças em estruturas compartilhadas podem exigir atualização de:

* testes;
* documentação;
* serialização;
* eventos;
* observadores;
* visualização.

Decisões estruturais duradouras devem ser registradas no `decision-log.md`.
