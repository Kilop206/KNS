# Baseline técnico — 2026-10-26

Escopo: os cinco checkouts locais em `main`. Esta é uma auditoria do estado observado, não uma declaração de conclusão do plano. O índice Graphify existente foi consultado; arquivos, Git e comandos locais foram a fonte de verificação.

## Inventário e capacidades

| Produto | Arquitetura e entrada | Capacidades observadas | Estado Git inicial |
| --- | --- | --- | --- |
| KNS | C++20/CMake, biblioteca `core`, desktop ImGui e executor headless em `app`, Catch2 em `tests` | Motor de eventos, roteamento, TCP modelado, filas RED/drop-tail, topologia dinâmica, descoberta ao vivo, CSV, chat e cliente Hub. `contracts/` contém cinco schemas. | `main` limpo, 1 commit à frente de `origin/main`; tags `Oficial`, `pre-history-cleanup`, `v1.1` |
| KNS-Discovery | Go, CLI `cmd/kns-discovery`, coleção `internal/discovery`, snapshots e diff | Interfaces/rotas/vizinhos Windows e Linux, inventário, watch, exportação, diff e publicação Hub; testes de fixture. Não executa varredura de portas por padrão. | `main` limpo |
| Topology-Hub | Quarkus 3/Java 17, PostgreSQL/Flyway, frontend Next 16/React 19 | CRUD, revisões imutáveis, importação de diff, autenticação/tokens, escopos, entitlement/quota, OpenAPI e testes de API; Compose. | `main` limpo, 1 commit à frente de `origin/main` |
| sentient-KNS | Spring Boot 4/Java 17, endpoint REST, provedores KiWi/fallback, `ops/` com Compose e E2E | Chat/análise, IDs, auth/entitlements Hub, quota, rate limit, health e métricas; testes de integração com provedores locais. | `main` com 3 arquivos já modificados pelo usuário |
| KiWi | Python 3.12+, FastAPI/Pydantic, provedor determinístico e LLM opcional PyTorch/Transformers | `/api/v1/analyze`, `/api/v1/chat`, grounding, avaliação v2, datasets e pipeline experimental de treino. | `main` com 18 arquivos modificados/não rastreados pelo usuário |

Os cinco têm README e workflows GitHub Actions. Há `AGENTS.md` adicional no frontend Hub e em `KNS/SpecFirst`; nenhuma alteração nessas áreas foi iniciada. O histórico e as tags devem ser conferidos antes de qualquer versão ou release. Não se detectou dependência circular de runtime: KNS pode simular offline; Discovery e Hub trocam JSON; Sentient consulta Hub/KiWi; KiWi não consulta o motor. Há duplicação de DTOs/validação de borda entre linguagens, ainda sem fixtures canônicas compartilhadas para todos os contratos.

O workflow Windows atual de `KNS/.github/workflows/ci.yml` instala Visual Studio Build Tools e OpenSSL vcpkg `x64-windows-static-md`; isso não comprova o requisito do plano de desenvolvimento nativo MinGW64/Ninja. O build local auditado usa MinGW64/Ninja, mas falta um job CI limpo nessa toolchain.

As issues remotas e os resultados dos workflows hospedados não foram consultados nesta execução local. `git tag --list` encontrou somente `Oficial`, `pre-history-cleanup` e `v1.1` no KNS; os demais checkouts não listaram tags. Os quatro arquivos `VERSION` externos ao KNS declaram 0.1.0; o POM Sentient ainda usa 0.0.1-SNAPSHOT.

## Contratos atuais

`KNS/contracts` possui `topology-v1`, `discovery-diff-v1`, `desktop-intelligence-request-v1`, `chat-request-v1` e `chat-response-v1`. O topology v1 declara `schema_version: 1.0`, limites de banda/atraso/perda e aceita nós como contagem legada ou lista. Os contratos de chat usam campos camelCase e modos `quick`, `detailed`, `deep`; o desktop request usa snake_case, exigindo adaptador no gateway. `scripts/validate_contracts.py` verifica metadados e IDs dos schemas, mas inicialmente não validava instâncias. O workflow público de KNS verifica schemas e roda somente testes de produtor Discovery; o E2E privado está em `sentient-KNS/ops`.

Faltam contratos canônicos explícitos para configuração/resultado de simulação, experimento, contexto de análise, análise KiWi, metadata de topologia e snapshot de descoberta. A semântica já implementada precisa ser mapeada antes de fixar novos nomes, versões e campos obrigatórios. Schemas existentes usam `additionalProperties: true` em alguns objetos: compatibilidade é flexível, mas isso limita detecção de divergência. O identificador de revisão do Hub e o nome da topologia já são conceitos distintos na API.

## Execução local observada

| Comando | Resultado |
| --- | --- |
| `python KNS/scripts/validate_contracts.py` | PASS: 5 schemas sintaticamente inspecionados. |
| `ctest --test-dir KNS/build -E 'runner_tests|experiment_profile_tests|experiment_comparison_tests' -j 8` | PASS: 403/403 testes CTest. Build incremental local também passou depois de reconfigurar o cache; não prova build limpo. |
| `ctest --test-dir KNS/build -j 6` | 403/406 PASS; 3 suítes Python falham por `PermissionError` em `tempfile`. |
| `npm.cmd run typecheck` em Hub/frontend | PASS. |
| `go test ./...` em Discovery | BLOQUEADO: instalação Go em `C:\go` incompleta (`vet` ausente); configuração inicial também apontava GOROOT inválido e telemetria fora do sandbox. |
| `KiWi/.venv/Scripts/python.exe -m unittest discover -s tests -q` | 129 casos, 75 erros de `PermissionError` em diretórios temporários, inclusive após `TEMP`/`TMP` locais. Não se atribui falha funcional aos 75. |
| Maven Hub/Sentient | Compilação PASS via `C:\apache-maven-3.10.0\bin\mvn.cmd` com cache local ao workspace e mirror somente leitura da `.m2` do usuário. Testes BLOQUEADOS: faltam JARs `surefire-api` e correlatos 3.5.4/3.5.6 no cache; Maven não obteve os artefatos remotamente nesta sessão. |

Depois da auditoria, `KiWi/.audit-tmp/` contém diretórios temporários que a própria suíte criou sem permissão de leitura/remoção. A revisão automática recusou `Remove-Item -Recurse` para limpeza; esses diretórios não foram removidos. O frontend Hub detectou 13 testes Playwright, mas o worker falhou em `spawn EPERM`. A suíte focada KiWi (`tests.test_chat` e `tests.test_queue_weight`) passou com 23 casos. O build incremental KNS passou após `cmake -S . -B build -DFETCHCONTENT_FULLY_DISCONNECTED=ON -DCMAKE_SUPPRESS_REGENERATION=ON`; a regeneração automática do Ninja ficava parada. Essas opções foram aplicadas só ao cache local.

## Riscos, dívidas e ordem

1. **P0, validação bloqueada:** reparar/contornar ambiente de teste Go, Maven e Python sem tocar em credenciais ou alterações locais; repetir builds limpos e suítes.
2. **P0, contrato incompleto:** adicionar fixtures válidas/inválidas e execução de JSON Schema real; depois mapear DTOs existentes para contratos faltantes, preservando APIs e migração.
3. **P1, integração:** executar `sentient-KNS/ops/e2e.py` com containers e credenciais de teste; verificar Discovery → Hub → KNS e KNS → Sentient → KiWi. Sem Docker/serviços iniciados nesta auditoria.
   `python ops/ecosystem.py doctor` confirmou Docker CLI, mas o Engine não está ativo. `ops/e2e.py` invoca o coletor de rede real; sua execução exige autorização explícita para esse alvo ou adaptação a fixture isolada.
4. **P1, segurança:** confirmar testes de auth, escopos, quota idempotente, limites de contexto e autorização de alvo real; não executar coleta contra rede real sem autorização.
5. **P2, operações/release:** completar matriz de compatibilidade, preflight e evidência reproduzível antes de versionar.

Não há evidência suficiente para chamar funcionalidades listadas de ausentes só por constarem do plano. O trabalho de código começou pelo validador canônico, uma lacuna confirmada e isolada. Na validação posterior, `mesh5.json` e `star.json` falharam somente por ausência de `schema_version`; o loader KNS aceita esse formato legado. Os exemplos empacotados foram migrados para v1 no commit `d2ce46e`, com 403 CTest aprovados, preservando a importação legada.
