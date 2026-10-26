# Decisões

## DEC-001 — Preservar contratos v1 e checkouts locais

Data: 2026-10-26. `KNS/contracts` já é fonte canônica documentada. A primeira correção será validação de instâncias e fixtures, preservando formatos publicados. Sentient e KiWi têm alterações pré-existentes; nenhum arquivo modificado desses checkouts será sobrescrito. Não há autorização para varrer rede real ou modificar credenciais.

## DEC-002 — Separar falha de produto de falha de ambiente

Data: 2026-10-26. Suítes que falham por cache de usuário, toolchain incompleto ou diretório temporário negado permanecem BLOCKED até execução válida. Os 403 testes CTest aprovados são evidência apenas do build preexistente e das suítes executadas.

## DEC-003 — Documentação do workspace sem Git próprio

Data: 2026-10-26. A raiz `KNS Environment` não é um repositório Git. Os arquivos em `docs/implementation/` foram criados no caminho exigido pelo plano. Para versioná-los sem criar um sexto produto, `KNS/docs/implementation/` passa a ser a fonte Git canônica; a pasta da raiz é um espelho local de trabalho. Mudanças futuras nesses documentos devem ser aplicadas à fonte versionada e sincronizadas ao espelho da raiz.

## DEC-004 — Divergência de IDs e limites de topologia

Data: 2026-10-26. Auditoria de código identificou limite de 4096 nós e IDs densos no KNS, limite de 4096 na Discovery e aceitação de até 5000 nós e IDs esparsos no Hub. O schema v1 atual aceita ambos. Uma correção que rejeite topologias anteriormente aceitas pelo Hub requer plano de compatibilidade; uma correção que amplie o KNS requer teste de escala. Não foi feita alteração pública neste ponto.

## DEC-005 — Contratos novos seguem produtores existentes

Data: 2026-10-26. `kiwi.analysis`, `analysis.context` e `discovery.snapshot` descrevem payloads reais atuais, com suas lacunas registradas. O `summary.json` agregado de `scripts/run.py` não será chamado de `simulation.result` por execução. Perfis de benchmark não serão equiparados a propostas de experimento aprovado. Os contratos restantes só serão publicados junto de produtor/consumidor ou migração verificável.

## DEC-006 — Windows usa MinGW64 e TLS deve falhar sem OpenSSL

Data: 2026-10-26. CI e release Windows foram alinhados ao MinGW64/Ninja, com OpenSSL da mesma toolchain. A opção `KNS_ENABLE_TLS=ON` antes não ativava OpenSSL em cpp-httplib, apesar da documentação e do teste Linux; agora a configuração exige a biblioteca e registra o teste HTTPS em Windows. Não declarar o milestone concluído até build limpo, testes de certificados e execução do pacote no runner hospedado. O build local com TLS travou, e `test_https.py` não conseguiu criar certificados no temporário deste ambiente.

## DEC-007 — Trust store explícita para clientes HTTPS do desktop

Data: 2026-10-26. O cpp-httplib usa o repositório de certificados do Windows por padrão; o teste hospedado mostrou que `SSL_CERT_FILE` não era aplicado automaticamente à CA do servidor de teste. KNS agora configura `SSL_CERT_FILE` ou `SSL_CERT_DIR` explicitamente nos clientes Hub, inteligência e tradução quando definidos, preservando o trust store do sistema na ausência dessas variáveis. Verificação de certificado e hostname permanece ativa. `desktop_https` passou no runner MinGW64 após `c67f03d` em [CI 40363759606](https://github.com/Kilop206/KNS/actions/runs/40363759606).
