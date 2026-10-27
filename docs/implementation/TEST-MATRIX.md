# Matriz de testes

| Camada | Comando | Resultado em 2026-10-26 |
| --- | --- | --- |
| KNS schemas e instâncias | `python KNS/scripts/validate_contracts.py` | 8 schemas, 24 fixtures e 4 topologias empacotadas PASS; produtor Go validado em CI Discovery |
| Snapshot Discovery existente | JSON Schema com registry local sobre `KNS-Discovery/output/network.json` | PASS; arquivo preexistente lido sem coleta nova ou exposição do conteúdo |
| Discovery → KNS offline | `KNS.exe --headless --topology ../KNS-Discovery/output/network.json` em diretório isolado | exit 0; contexto gerado validou `analysis.context.schema.json`; não testou Hub ou coleta nova |
| Fluxo sintético de três produtos | `KiWi/.venv/Scripts/python.exe sentient-KNS/ops/offline_contract.py --work-dir KNS/build` | PASS: fixture de descoberta, KNS headless, contexto real e resposta KiWi validados; gate privado atualizado |
| Produtor real de contexto KNS | `python KNS/scripts/validate_analysis_context.py --work-dir KNS/build` | KNS headless exit 0; `AIContextBuilder` emitiu contexto `detailed` validado contra o novo schema |
| Timestamp do resumo KNS | `run.write_summary_json([], build/run-summary-check, {}, [])` e parse de `generated_at` | ISO 8601 com `Z` e offset UTC zero PASS; suite Python completa ainda bloqueada por temp |
| KNS → KiWi direto | `TestClient` POST `/api/v1/analyze` com contexto emitido pelo KNS e `KIWI_LLM_ENABLED=false` | HTTP 200; resposta validada por `kiwi.analysis.schema.json`; não inclui Sentient |
| KiWi analysis response | `AiAnalysisResponse.model_validate` sobre fixtures canônicas | resposta válida aceita; severidade inválida rejeitada |
| Limites de interoperabilidade | `KNS.exe --headless --topology build/contract-boundaries/{sparse,oversize}.json` e validador JSON Schema | Schema aceitou ambos; KNS rejeitou IDs 10/20 e 4097 nós com exit code 1; comportamento Hub confirmado em `GraphsTest` por código, não executado |
| Remapeamento de IDs esparsos KNS | `g++` compila programa local com `TopologyLoader.cpp` alterado e `build/core/libkns.a`; execução `build/sparse_check.exe` | PASS: importou IDs 10/20, exportou 0/1, rejeitou link para 30. Compilação sintática dos dois arquivos alterados passou; suíte Catch2 completa aguarda CI. Ninja local parou antes do compilador |
| KNS C++/integração local | `ctest --test-dir KNS/build -E 'runner_tests|experiment_profile_tests|experiment_comparison_tests' -j 8` | 403/403 PASS, build existente |
| KNS recompilação incremental | `cmake -S KNS -B KNS/build -DFETCHCONTENT_FULLY_DISCONNECTED=ON -DCMAKE_SUPPRESS_REGENERATION=ON`, depois `cmake --build KNS/build --parallel 6` | configuração PASS; Ninja `no work to do`; não é build limpo |
| KNS CI hospedada | [CI 40361835152](https://github.com/Kilop206/KNS/actions/runs/40361835152) | macOS, Ubuntu e warnings-as-errors PASS; Windows MinGW64 compilou e passou 406/407 CTest, mas `desktop_https` rejeitou a CA confiável; sanitizers interrompido pelo runner na compilação (`exit 143`). `e308915` e `c67f03d` aguardam revalidação hospedada |
| KNS CI após correções | [CI 40363759606](https://github.com/Kilop206/KNS/actions/runs/40363759606) | PASS nos cinco jobs: Windows MinGW64/Ninja 407/407 CTest inclusive `desktop_https`, 8 schemas/24 fixtures/4 topologias e contexto; Ubuntu, macOS, sanitizers e warnings PASS |
| KNS HTTPS macOS | [CI 40369024451](https://github.com/Kilop206/KNS/actions/runs/40369024451) | PASS nos cinco jobs; macOS 407/407 CTest, inclusive `desktop_https`, e 8 schemas/24 fixtures/4 topologias; Windows MinGW64, Ubuntu, sanitizers e warnings PASS |
| Sintaxe do cliente HTTPS KNS | `g++ -std=c++20 -D_WIN32_WINNT=0x0A00 -DWINVER=0x0A00 -fsyntax-only` sobre `app/http/ClientTrust.hpp`, com e sem `CPPHTTPLIB_OPENSSL_SUPPORT` | PASS nos dois modos; não substitui build/link e CTest HTTPS hospedados |
| Windows MinGW CI/release | YAML parse de `ci.yml` e `release.yml`; `objdump -p KNS.exe`; smoke headless com apenas `KNS.exe` e DLLs MinGW em PATH isolado | parse PASS; binário local saiu 0 com `libgcc_s_seh-1`, `libstdc++-6` e `libwinpthread-1`; runner hospedado e pacote OpenSSL ainda não executados |
| Configuração HTTPS desktop | `cmake -S KNS -B KNS/build -DKNS_ENABLE_TLS=ON -DOPENSSL_ROOT_DIR=C:/openssl-4.0.3/x64` | CMake PASS, OpenSSL 4.0.3 encontrado, macros HTTPS presentes; rebuild não concluiu; `test_https.py` falhou na criação de certificados por permissão de temporário, antes de testar o binário |
| KNS Python experimentos | `ctest --test-dir KNS/build -j 6` | 3 falhas por temp inacessível |
| Discovery Go | `go test ./...` | bloqueado por instalação Go incompleta |
| Discovery CI preexistente | [Go validation](https://github.com/Kilop206/KNS-Discovery/actions/runs/39588666865) | PASS em Ubuntu/Windows: contratos de diff, `go test`, `go vet`, `go build`; serviu de baseline anterior ao gate de snapshot |
| Discovery snapshot Go → schema | [CI 40365781532](https://github.com/Kilop206/KNS-Discovery/actions/runs/40365781532) | PASS em Ubuntu/Windows: fixture Go serializada validou `discovery.snapshot.schema.json`, fixture canônica inválida foi rejeitada; `go test ./...`, `go vet ./...` e `go build` PASS; não houve coleta real |
| Discovery `observed_at` UTC | [CI 40367388593](https://github.com/Kilop206/KNS-Discovery/actions/runs/40367388593) | PASS em Ubuntu/Windows: produtor de fixture com offset convertido para UTC, publicação retém horário em grafo idêntico, contrato canônico, `go test ./...`, `go vet ./...`, `go build`; coleta real não executada |
| Hub frontend TS | `npm.cmd run typecheck` | PASS |
| Hub frontend browser | `npm.cmd test` | 13 testes detectados, worker bloqueado por `spawn EPERM` |
| Hub backend | Maven `compile -q` com mirror file e cache em `backend/target/audit-m2` | PASS; `test` bloqueado por `surefire-api` 3.5.4 ausente do cache |
| Hub CI hospedada | [Validate Topology Hub](https://github.com/Kilop206/Topology-Hub/actions/runs/40324837376) | PASS: backend `mvn verify`, frontend typecheck/build e E2E com PostgreSQL e Playwright; esta execução não é o E2E dos cinco produtos |
| Hub consome snapshot Discovery UTC | [CI 40485072350](https://github.com/Kilop206/Topology-Hub/actions/runs/40485072350) | PASS: fixture canônica positiva/negativa, `mvn verify` com 30 testes (incluindo timestamp e revisão), frontend e 13 Playwright com PostgreSQL; gate local Python com contratos irmãos PASS, Maven local indisponível |
| Sentient | Maven `compile -q` com mirror file e cache em `target/audit-m2` | PASS; `test` bloqueado por `surefire-api`, `surefire-extensions-api` e `maven-surefire-common` 3.5.6 ausentes |
| Sentient CI hospedada | [Validate Sentient KNS](https://github.com/Kilop206/sentient-KNS/actions/runs/40324844408) | PASS em Ubuntu e Windows: Maven `verify`, contratos canônicos e release train; não exercita Hub/KiWi reais juntos |
| KiWi | `.venv/Scripts/python.exe -m unittest discover -s tests -q` | 129 casos, 75 erros de permissão em temp |
| KiWi focado | `.venv/Scripts/python.exe -m unittest tests.test_chat tests.test_queue_weight -q` | 23 PASS |
| KiWi CI hospedada | [KiWi evaluation regression](https://github.com/Kilop206/KiWi/actions/runs/40324848806) | PASS em Ubuntu/Windows e Python 3.12/3.14: unittest completo, contratos, gerador de candidatos e avaliação determinística |
| KiWi avaliação determinística | `.venv/Scripts/python.exe -m evaluation.runner --fail-on-issues` | 30/30 casos limpos, 0 issues; relatórios gerados foram restaurados ao estado Git inicial após a verificação |
| Compatibilidade chat KNS → KiWi | `ChatRequest`/`ChatResponse.model_validate` sobre quatro fixtures canônicas | 2 válidas aceitas, 2 inválidas rejeitadas |
| Release preflight | `python KNS/scripts/validate_release_train.py` | PASS: candidato `kns-ecosystem-1`, modelo `pipeline_ready` |
| Release preflight estrutural | `python KNS/scripts/release_preflight.py --structural-only` | estrutura PASS; software não pronto sem evidência E2E privada; modelo ainda `pipeline_ready` |
| Diagnóstico E2E | `python sentient-KNS/ops/ecosystem.py doctor` | 5 checkouts e ferramentas encontrados; Docker Engine indisponível |
| E2E cinco produtos | `sentient-KNS/ops/e2e.py` | não executado; Docker Engine indisponível e o script usa coletor Discovery real |

Resultados não executados ou bloqueados não contam como aprovação. Logs completos do CTest ficam no build local do KNS.
