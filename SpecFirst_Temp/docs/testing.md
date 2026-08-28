# Estratégia de Testes — KNS

## 1. Objetivo

Os testes do KNS existem para demonstrar que o comportamento do simulador continua correto à medida que o projeto evolui.

A estratégia privilegia:

* testes determinísticos;
* testes de regressão;
* testes unitários de regras isoladas;
* testes de integração entre componentes.

---

## 2. Framework

O projeto utiliza:

```text
Catch2
CTest
catch_discover_tests
```

O `tests/CMakeLists.txt` registra os testes automaticamente através de `catch_discover_tests`.

---

## 3. Tipos de teste

## 3.1 Testes unitários

Usar para comportamentos locais e determinísticos.

Exemplos:

* `SimulationClock`;
* `EventQueue`;
* `Link`;
* `Topology`;
* `Routing`;
* máquinas de estado;
* conversões;
* helpers;
* regras de validação.

---

## 3.2 Testes de integração

Usar quando o comportamento depende da interação entre componentes.

Exemplos:

```text
Topology
   ↓
Routing
   ↓
SimulationEngine
   ↓
Link
   ↓
PacketReceivedEvent
```

ou:

```text
TCPConnection
   ↓
TCPSession
   ↓
TCP event
   ↓
Packet
   ↓
Network
```

---

## 3.3 Testes de regressão

Bugs corrigidos devem ganhar testes de regressão quando o comportamento for testável de forma automatizada.

O teste deve demonstrar o comportamento esperado e impedir que o bug volte a ocorrer.

---

## 4. Determinismo

O mesmo cenário deve produzir resultados equivalentes quando executado novamente.

Testes não devem depender de:

* relógio de parede;
* delays reais;
* GUI;
* ordem não especificada de containers;
* estado externo não controlado.

A simulação deve ser exercitada através do relógio lógico e dos eventos do engine.

---

## 5. Estratégia para bugs

Para uma issue de bug:

```text
reproduzir
   ↓
testar comportamento esperado
   ↓
corrigir
   ↓
executar regressão
```

Quando possível, o teste deve falhar antes da correção e passar depois dela.

---

## 6. Exemplo: transmissão

Para mudanças em transmissão de pacotes, validar pelo menos:

* rota encontrada;
* Link correto selecionado;
* transmissão aceita;
* transmissão rejeitada;
* pacote em trânsito;
* evento de recebimento;
* atualização de estatísticas;
* comportamento da fila.

---

## 7. Exemplo: fila FIFO

Uma implementação de fila deve ser testada com mais de um pacote.

Exemplo conceitual:

```text
P1 entra
P2 entra
P3 entra

ordem esperada:

P1
P2
P3
```

Também validar:

* capacidade máxima;
* tentativa de inserção quando cheia;
* remoção;
* consistência do tamanho;
* comportamento em FULL_DUPLEX;
* comportamento em HALF_DUPLEX quando aplicável.

---

## 8. Exemplo: TCP

Para mudanças TCP, validar as transições relevantes.

Handshake:

```text
CLOSED
  ↓
SYN_SENT
  ↓
ESTABLISHED
```

Peer:

```text
CLOSED
  ↓
SYN_RECEIVED
  ↓
ESTABLISHED
```

Encerramento:

```text
ESTABLISHED
  ↓
FIN_WAIT_1
  ↓
FIN_WAIT_2
  ↓
TIME_WAIT
```

e o caminho passivo correspondente.

---

## 9. Comandos oficiais

Configuração:

```bash
cmake -S . -B build
```

Build:

```bash
cmake --build build
```

Testes:

```bash
ctest --test-dir build --output-on-failure
```

No Windows, o executável de testes também pode ser executado diretamente quando necessário:

```powershell
.\build\tests\kns_tests.exe
```

---

## 10. Build limpo

Quando uma alteração puder sofrer influência de artefatos anteriores, utilizar uma configuração limpa.

Exemplo:

```bash
cmake -S . -B build-clean
cmake --build build-clean
ctest --test-dir build-clean --output-on-failure
```

O objetivo é evitar que problemas de configuração antigos sejam confundidos com problemas do código atual.

---

## 11. Toolchain

No Windows, a configuração deve ser coerente com o toolchain definido pelo projeto.

O ambiente alvo utiliza:

```text
C:\mingw64
```

Não considerar um build como validado apenas porque foi compilado com outro runtime incompatível.

---

## 12. Definition of Done para testes

Uma tarefa com alteração de comportamento deve, quando aplicável:

* ter teste novo ou atualizado;
* passar nos testes relevantes;
* passar no build;
* preservar testes existentes;
* não introduzir regressões conhecidas.

Quando algum check não puder ser executado:

1. registrar a impossibilidade;
2. explicar o motivo;
3. registrar o risco residual;
4. não apresentar a tarefa como totalmente validada.

---

## 13. O que não é um teste suficiente

Não considerar como validação suficiente:

* apenas compilar;
* apenas executar a GUI manualmente;
* apenas observar que um pacote apareceu;
* apenas testar o caminho feliz;
* alterar o teste para aceitar o comportamento atual sem justificar.

O teste deve demonstrar o contrato que o sistema precisa cumprir.

---

## 14. Relação com issues

Uma issue de bug só deve ser marcada como concluída quando:

```text
implementação
+
teste de regressão
+
checks
+
documentação
```

estiverem sincronizados com o estado real do projeto.

---

## 15. Validação mínima por categoria

### Simulation

* ordenação temporal;
* desempate determinístico;
* processamento de eventos;
* estado da fila.

### Network

* topologia;
* Links;
* modos duplex;
* delay;
* bandwidth;
* loss;
* capacidade;
* FIFO;
* roteamento.

### TCP

* estados;
* handshake;
* sequence;
* ACK;
* DATA;
* FIN;
* encerramento;
* timeouts já implementados.

### Application

* GUI;
* headless;
* carregamento de topologia;
* controles;
* integração com o engine.

---

## 16. Regra final

> **Todo comportamento importante deve possuir uma forma objetiva de ser validado.**

Quando um comportamento não puder ser validado automaticamente, a documentação deve explicar como ele será verificado e quais limitações permanecem.
