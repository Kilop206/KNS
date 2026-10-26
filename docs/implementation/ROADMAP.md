# Roadmap e backlog priorizado

Status permitido: TODO, IN PROGRESS, BLOCKED, DONE, VERIFIED. `VERIFIED` exige evidência de teste real.

| ID | Prioridade | Dependências | Tarefa e critério de aceite | Testes/evidência | Status |
| --- | --- | --- | --- | --- | --- |
| AUD-001 | P0 | nenhuma | Auditar cinco repositórios e registrar baseline sem sobrescrever mudanças locais | `BASELINE.md`, `git status`, comandos registrados | VERIFIED |
| ENV-001 | P0 | AUD-001 | Dispor de Go completo, Maven executável e temporários Python graváveis; repetir suítes | `go test ./...`, Maven test, unittest, CTest total | BLOCKED |
| CI-001 | P1 | AUD-001 | Validar KNS em Windows com MinGW64/Ninja e OpenSSL da mesma toolchain, sem exigir MSVC | workflows CI/release migrados em KNS `c11e4f8`; 403 CTest locais PASS e pacote MinGW local executou isolado; falta execução limpa no runner Windows | IN PROGRESS |
| TLS-001 | P1 | CI-001 | Exigir suporte HTTPS quando `KNS_ENABLE_TLS=ON` e testar certificados nos desktops suportados | KNS `e257646`; CMake local encontrou OpenSSL 4.0.3 e ativou `CPPHTTPLIB_OPENSSL_SUPPORT`; rebuild TLS local travou, `test_https.py` falhou antes da chamada ao KNS por permissão de temporário; falta CTest HTTPS em CI | IN PROGRESS |
| CON-001 | P0 | AUD-001 | Validar instâncias positivas/negativas dos cinco contratos existentes | 5 schemas e 15 fixtures aprovados por `python scripts/validate_contracts.py`; workflow atualizado; KNS `1523a02`, `fd2def6` | VERIFIED |
| CON-002 | P0 | CON-001 | Inventariar payloads reais e definir schemas faltantes sem quebrar v1 | teste por produtor/consumidor e ADR se necessário | IN PROGRESS |
| CON-004 | P1 | CON-001 | Topologias de exemplo KNS conformes ao v1 mantendo importação legada | 4 exemplos/15 fixtures/5 schemas e 403 CTest PASS; KNS `d2ce46e`, CI `4dc1171` | VERIFIED |
| CON-005 | P0 | CON-002 | Resolver limite de nós e densidade de IDs entre KNS, Discovery, Hub e schema v1 sem quebra silenciosa | fixtures de 4096/4097 nós e IDs esparsos; teste de importação KNS e API Hub; plano de migração | TODO |
| CON-006 | P1 | CON-001 | Documentar unidades e identidade no schema de topologia sem mudar validação | 5 schemas/15 fixtures/4 exemplos PASS; KNS `1dc9ec7` | VERIFIED |
| CON-007 | P1 | CON-002 | Descrever resposta `/api/v1/analyze` existente sem exigir migração imediata | 6 schemas/17 fixtures/4 exemplos PASS; Pydantic KiWi aceitou válida e rejeitou inválida; KNS `e4aff8e` | VERIFIED |
| CON-008 | P1 | CON-002 | Descrever e validar contexto real emitido pelo KNS headless | 7 schemas/19 fixtures/4 exemplos PASS; headless exit 0, contexto emitido validou; CI instala `jsonschema`; KNS `c0467df` | VERIFIED |
| CON-009 | P1 | CON-002, ENV-001 | Validar snapshot Go contra perfil de descoberta v1 e timestamp UTC em migração futura | schema/ref/fixtures aprovados (8 schemas/24 fixtures), KNS `105dd59`, `4c84034`; `output/network.json` existente validou sem nova coleta; falta suíte Go | IN PROGRESS |
| E2E-002 | P1 | CON-007, CON-008, fixture de CON-009 | Fluxo offline sintético Discovery → KNS → KiWi com validação de contratos, sem serviços | script `ops/offline_contract.py` PASS; KNS `4c84034`, Sentient `86178f4`; integrado em `ops/verify.py` | VERIFIED |
| ENG-002 | P1 | AUD-001 | Tornar o timestamp do resumo JSON existente explicitamente UTC | `write_summary_json` gerou ISO 8601 com `Z` e offset zero; KNS `1d1156a` | VERIFIED |
| CON-003 | P0 | CON-002, ENV-001 | Todos os produtores/consumidores validam fixtures compartilhadas e rejeitam versões inválidas | CI entre repositórios | TODO |
| ENG-001 | P1 | CON-002, ENV-001 | Resultado/configuração reproduzíveis com versão, seed, revisão e hash | CTest, replay e serialização | TODO |
| DSC-001 | P1 | CON-003, ENV-001 | Snapshot de fixture valida e importa no KNS sem inventar links | snapshot preexistente validou e executou KNS headless exit 0; faltam Go test e coleta de fixture reproduzível | IN PROGRESS |
| HUB-001 | P1 | CON-003, ENV-001 | Revisões imutáveis e permissões validadas em PostgreSQL | API + E2E PostgreSQL | TODO |
| AI-001 | P1 | CON-003, ENV-001 | Análise KiWi com fatos rastreáveis e fallback seguro | unittest + avaliação determinística | TODO |
| SEN-001 | P1 | AI-001, HUB-001 | Chat correlacionado, autorizado e quota idempotente | integração Spring/Hub/KiWi | TODO |
| E2E-001 | P1 | ENG-001, DSC-001, HUB-001, SEN-001 | Fluxos A–E em Compose com fixture, seed e falhas simuladas | `ops/e2e.py` e relatório; `doctor` encontrou Docker Engine indisponível; execução atual coleta rede real | BLOCKED |
| SEC-001 | P1 | E2E-001 | Revisão de segredos, escopos, entradas e coleta autorizada | testes negativos | TODO |
| REL-001 | P2 | E2E-001, SEC-001 | Documentação, compatibilidade e checklist de release completos | preflight e tags | TODO |
| DOC-001 | P1 | AUD-001 | Versionar o plano do workspace sem criar um sexto produto | `KNS/docs/implementation/` é fonte Git canônica; pasta da raiz é espelho local; 7 documentos copiados com SHA-256 idêntico | VERIFIED |

## Evidência de milestones nesta execução

- **Milestone 0 / AUD-001 — VERIFIED:** baseline, matriz e inventário dos cinco checkouts em `docs/implementation/`. A raiz do workspace não é Git; cópia versionada em `KNS/docs/implementation/` conforme DEC-003. Bloqueios: ambiente Go/Maven/Python/Playwright e Docker Engine.
- **Milestone 1 / CON-001, CON-004, CON-006, CON-007 e CON-008 — VERIFIED localmente:** KNS `1523a02`, `fd2def6`, `d2ce46e`, `4dc1171`, `1dc9ec7`, `e4aff8e`, `c0467df`; 8 schemas e 24 fixtures atualmente validados, 4 topologias de exemplo, build incremental, produtor de contexto headless e 403 CTest aprovados. CON-009 já tem contrato e fixture sintética em `105dd59`/`4c84034`, mas permanece IN PROGRESS até rodar o produtor Go. Bloqueios restantes: validação de consumidores Hub/Discovery em CI, build limpo e contratos ainda ausentes.
- **Milestone de integração offline / E2E-002 — VERIFIED:** `sentient-KNS/ops/offline_contract.py` passou com fixture sintética e foi adicionado ao gate privado de `ops/verify.py` no commit `86178f4`. Não exerce Hub, Sentient HTTP, PostgreSQL ou coleta real; o E2E completo segue BLOCKED.
- **CI Windows / CI-001 — IN PROGRESS:** `c11e4f8` migrou CI e release para MSYS2 MinGW64/Ninja; `2c9c760` adicionou smoke test isolado antes de publicar o pacote. 403 CTest locais no build anterior, contratos, contexto emitido, parse YAML e execução isolada do pacote local passaram. Ainda falta build limpo e CTest no runner hospedado.
- **TLS desktop / TLS-001 — IN PROGRESS:** `e257646` vincula a opção TLS ao OpenSSL exigido por cpp-httplib e registra `desktop_https` também no Windows; `8efd8a2` esclarece os pré-requisitos. A configuração local passou. O build local com TLS não concluiu e o teste Python encontrou bloqueio de permissão ao gerar certificados no temporário. O release Windows inclui DLLs OpenSSL; pacote e HTTPS ainda exigem validação hospedada.
- **Documentação / DOC-001 — VERIFIED:** os sete documentos em `docs/implementation/` foram copiados para a fonte versionada `KNS/docs/implementation/` e cada cópia teve SHA-256 conferido. DEC-003 registra que a pasta da raiz é espelho de trabalho.

Próxima ordem elegível: CI-001/TLS-001 (build limpo e `desktop_https` no runner MinGW64), CON-002 (migrar produtores antes de publicar contratos faltantes), CON-005 (compatibilidade de limites/IDs com migração), CON-003 após restabelecer Go/Maven. A inspeção dos DTOs do Hub confirmou que `topology.metadata` ainda precisa distinguir UUID da topologia, número de revisão e versão otimista, além de acrescentar hash persistido; nenhum schema hipotético foi publicado. E2E-001 depende do Docker Engine e de execução autorizada do coletor real ou de um modo isolado com fixtures.
