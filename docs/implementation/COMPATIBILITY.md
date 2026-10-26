# Compatibilidade observada

| Fluxo | Contrato atual | Prova local | Pendência |
| --- | --- | --- | --- |
| Discovery → KNS | `topology-v1.schema.json` | Testes CTest de importação e live snapshot aprovados | Go suite e fixture cross-repo |
| Discovery → Hub | Topologia v1 + `discovery-diff-v1` | Código e testes de publicação/API presentes | E2E PostgreSQL |
| Hub → KNS | API de topologia/revisões | Testes de cliente KNS aprovados | E2E com Hub real |
| KNS → Sentient | `desktop-intelligence-request-v1`, `chat-request-v1` | Testes do cliente KNS aprovados | Maven e fixture compartilhada |
| Sentient → KiWi | Adaptador REST `/api/v1/chat` e `/api/v1/analyze` | Código/testes presentes; Pydantic KiWi aceitou os dois fixtures canônicos positivos de chat e rejeitou os dois negativos em execução local. A API determinística KiWi aceitou contexto real emitido pelo KNS e respondeu conforme `kiwi.analysis` | Suites Maven/Python e E2E via Sentient |

O smoke test sintético versionado em `sentient-KNS/ops/offline_contract.py` cobre Discovery fixture → KNS → KiWi sem tocar na rede. `ops/verify.py` o executa na aceitação privada. O caminho completo ainda requer Hub, Sentient HTTP, PostgreSQL e a suíte E2E.

## Divergência confirmada de topologia v1

`KNS/core/src/network/TopologyLoader.cpp` limita nós a 4096 e, em lista tipada, exige IDs únicos no intervalo `[0, nodes.size())`. `KNS-Discovery/internal/discovery/build.go` usa `MaxNodes = 4096`. `Topology-Hub/backend/src/main/java/com/userex/hub/Graphs.java` aceita de 1 a 5000 nós e IDs inteiros únicos não negativos sem exigir densidade. O schema canônico atual aceita zero nós e não expressa limite superior ou densidade de IDs. Portanto um documento pode passar no Hub e no JSON Schema mas falhar ao importar no KNS; outro pode passar no KNS e no schema mas falhar no Hub. Essa divergência exige uma decisão de migração antes de alterar contratos públicos ou limitar a API do Hub.

Verificação executada: dois documentos sintéticos (`nodes: 4097` e IDs tipados `10,20`) passaram no JSON Schema e foram rejeitados pelo executável headless KNS com código 1. `GraphsTest.java` contém aceitação explícita de 5000 nós e IDs esparsos, mas a suíte Maven não rodou neste ambiente. O schema recebeu descrições de unidades/identidade em `1dc9ec7`; nenhum limite foi alterado.

Versões observadas: KNS CMake 1.1.0 com tag existente `v1.1`; Discovery, Hub, Sentient e KiWi têm `VERSION` 0.1.0 e nenhuma tag local listada. Hub Maven e KiWi pyproject declaram 0.1.0; Sentient Maven ainda declara 0.0.1-SNAPSHOT. O candidato `kns-ecosystem-1` passa em `KNS/scripts/validate_release_train.py`. Nenhuma versão foi alterada nesta sessão.
