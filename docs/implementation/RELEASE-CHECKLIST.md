# Checklist do trem `kns-ecosystem-1`

- [ ] Schemas canônicos completos, fixtures positivas/negativas e consumidores validados.
- [ ] Builds limpos e testes independentes dos cinco produtos.
- [ ] E2E A–E com PostgreSQL, credenciais de teste e LLM simulado.
- [ ] Autorização, quotas, segredos e coleta de rede revisados.
- [ ] Matriz de compatibilidade e procedimento de migração publicados.
- [ ] Tags existentes e versões conferidas; nenhuma versão duplicada ou reduzida.
- [ ] Evidências, commits convencionais e limitações registrados por milestone.

Em 2026-10-26, `release_preflight.py --structural-only` aprovou a estrutura e recusou declarar software pronto porque falta evidência privada de execução E2E. O estado do modelo é `pipeline_ready`, sem promoção.
