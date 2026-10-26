# Contratos canônicos

Fonte v1: `KNS/contracts`. Contratos existentes na auditoria: topologia, diff de descoberta, requisição de inteligência desktop, chat request e chat response. `kiwi.analysis.schema.json` foi acrescentado em `e4aff8e` para descrever a resposta atual de `/api/v1/analyze`, com fixtures positiva/negativa conferidas pelo Pydantic KiWi. `analysis.context.schema.json` foi acrescentado em `c0467df` e validado contra JSON emitido pelo KNS headless. `discovery.snapshot.schema.json` foi acrescentado em `105dd59` como perfil de topologia v1 para o Go atual; a suíte produtora Go ainda não rodou neste ambiente. Schemas e exemplos do consumidor são insumos de compatibilidade; não criar versões novas apenas para uniformizar nomes.

Regras já documentadas em `KNS/contracts/README.md`: campos opcionais aditivos podem entrar na mesma versão maior; remoção, renomeação, obrigatoriedade ou mudança semântica exigem nova versão maior; consumidores rejeitam versões maiores não suportadas. A auditoria ainda não demonstrou que cada consumidor valida exatamente o schema canônico.

Lacunas a especificar após mapear os DTOs: `simulation.config`, `simulation.result`, `experiment` e `topology.metadata`. O novo `kiwi.analysis` descreve somente o corpo legado existente, sem IDs ou versão no corpo; o caminho `/api/v1/analyze` identifica a versão. O contexto v1 separa análises estruturais calculadas pelo KNS de métricas de tráfego; ainda não transporta revisão/hash/proveniência completos. O snapshot atual do Go não contém carimbos temporais de observação; `observed_at` UTC é opcional até a migração do produtor. Futuras respostas envelopadas com correlação, evidência e proveniência requerem migração documentada. Cada novo contrato deverá fixar origem do dado (observado, simulado, inferido ou gerado por IA), unidade, UTC, IDs, revisão, hash, proveniência e erros estruturados. A política de migração para payloads legados deve acompanhar cada schema.

## Mapeamento de fontes já implementadas

| Contrato proposto | Fonte de dados atual | Cuidados de compatibilidade |
| --- | --- | --- |
| `discovery.snapshot` | `KNS-Discovery/internal/discovery/model.go`, serialização `Build`, topologia v1 | Perfil definido em `105dd59`; o snapshot atual é topologia v1 e não contém timestamp; produtor Go ainda requer verificação executável. |
| `simulation.config` | `KNS/core/include/engine/core/RunConfig.hpp` e opções do CLI | Seed e tamanho do pacote já existem; definir unidades e limite numérico conforme parser real. |
| `simulation.result` | CSV/headless KNS, estatísticas do motor e resumo JSON de `scripts/run.py` | O runner já produz `summary.json` agregado; falta resultado JSON canônico por execução ligado ao estado final do motor. |
| `experiment` | `KNS/scripts/experiment_profile.py` e `KNS Experiments/experiment-profile.json` | Perfil é de benchmark local; separar proposta aprovada da execução medida. |
| `analysis.context` | `KNS/core/include/analysis/AIContextBuilder.hpp` | Schema e teste do produtor real em `c0467df`; campos podem ser omitidos por profundidade/limite de contexto; ausência não é medição zero. |
| `kiwi.analysis` | `KiWi/app/models/request.py` e `response.py` | Resposta v1 documentada por schema em `e4aff8e`; `/api/v1/analyze` aceita camelCase e responde `summary/findings/recommendations`; manter adaptador Sentient. |
| `topology.metadata` | `Topology-Hub/backend/.../Topology.java`, `TopologyRevision.java`, `TopologyResource.java` | UUID da topologia, revisão numérica imutável e versão otimista têm funções distintas; hash de conteúdo ainda não foi observado no modelo. |

Os DTOs Pydantic KiWi restringem mensagens e contexto além do schema público de chat. Esse refinamento deve ser documentado e testado no gateway antes de endurecer o schema v1. O Hub grava `Instant` de criação e histórico de revisão, mas o mapeamento de proveniência e hash permanece uma tarefa de contrato e persistência.

## Próximos contratos: fonte, lacuna e dependência

| Contrato | Payload real hoje | Decisão necessária antes de publicar schema |
| --- | --- | --- |
| `simulation.config` | CLI KNS: `--topology`, `--seed` uint64, `--packet-size` bytes, `--routing-metric`, `--congestion-control`; `RunConfig` usa filename/seed/packet_size/auto_start/controle. | Definir envelope JSON e representação portátil de uint64 para consumidores JavaScript, junto de revisão/hash da topologia. Não existe entrada JSON de configuração hoje. |
| `simulation.result` | CSV KNS com `schema_version=1`, contagens, latência, duração em segundos e seed; `scripts/run.py` grava `summary.json` agregado; `AnalysisJsonSerializer` exporta análise estrutural separada. | Conectar JSON canônico por execução ao mesmo estado final do motor, explicitar `total_latency`/`avg_latency` em segundos e diferenciar métricas de tráfego de análise estrutural. O resumo do runner já é um artefato JSON, mas não é o contrato de resultado por execução. |
| `experiment` | `experiment_profile.py` recebe perfil local sem `schema_version`; `benchmark_suite.py` usa suite JSON 1.0 e produz CSV/comparações. | Separar perfil de benchmark de proposta aprovada pelo usuário com hipótese, permissões, estado e revisão. Fixar migração dos perfis existentes antes de usar um único nome `experiment`. |
| `topology.metadata` | Hub `Summary`/`RevisionSummary`/`RevisionDetail` com UUID, revisão numérica, versão otimista, `Instant`, ator e origem. | Adicionar hash de conteúdo normalizado sem reescrever revisões; definir campos de proveniência e semântica de deduplicação. Não confundir revisão com `version` de concorrência. |

Os quatro schemas acima permanecem TODO até haver produtor/consumidor ou plano de migração validável. Essa decisão evita declarar um formato hipotético como contrato estável.
