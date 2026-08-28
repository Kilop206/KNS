# Segurança — KNS

## 1. Objetivo

O KNS não possui atualmente um modelo tradicional de autenticação/autorização de usuários como uma aplicação web.

As principais preocupações de segurança estão relacionadas a:

* credenciais de desenvolvimento;
* arquivos de configuração;
* execução de scripts;
* arquivos de topologia;
* dependências externas;
* logs;
* integridade do repositório;
* execução de ferramentas auxiliares.

---

# 2. Credenciais

Nunca armazenar no repositório:

* GitHub PATs;
* senhas;
* chaves privadas;
* tokens;
* credenciais de APIs;
* secrets de CI;
* arquivos de autenticação.

Credenciais devem ser fornecidas através de mecanismos externos apropriados.

---

# 3. Arquivos de topologia

Arquivos JSON de topologia são entradas externas e devem ser validados.

Nunca assumir que:

* número de nós é válido;
* endpoints de Links existem;
* valores numéricos são válidos;
* propriedades estão presentes;
* enumerações possuem valores conhecidos.

A validação deve ocorrer antes da criação de estruturas internas inconsistentes.

---

# 4. Scripts e automação

Scripts Python e outras ferramentas auxiliares não devem executar comandos destrutivos ou interpretar entrada externa de forma insegura sem validação.

Particular atenção deve ser dada a:

* caminhos de arquivos;
* argumentos CLI;
* nomes de arquivos;
* diretórios de resultados;
* parâmetros de experimentos.

---

# 5. Logs

Logs podem registrar informações úteis para depuração, como:

* timestamps;
* tipos de pacotes;
* nós;
* sessões;
* resultados de operações.

Não registrar:

* tokens;
* senhas;
* credenciais;
* dados externos desnecessários.

---

# 6. Dependências

Dependências externas devem ser adicionadas através do mecanismo oficial do CMake e permanecer fixadas/gerenciadas de forma reproduzível quando o projeto exigir.

Novas dependências relevantes devem passar por avaliação.

---

# 7. Build

O ambiente Windows alvo utiliza o toolchain definido para o KNS.

Não misturar:

```text
C:\mingw64
```

com runtimes incompatíveis como:

```text
C:\msys64\ucrt64
```

sem uma decisão explícita.

Mistura de runtimes pode produzir executáveis que funcionam apenas em determinados ambientes.

---

# 8. Dados gerados

Resultados de experimentos, gráficos e CSVs não devem conter segredos.

Arquivos temporários e artefatos gerados devem ser separados do código-fonte quando apropriado.

---

# 9. Segurança do modelo de IA

Agentes de IA não devem:

* solicitar ou registrar credenciais desnecessariamente;
* inserir secrets em arquivos;
* alterar controles de segurança sem justificativa;
* ocultar falhas de validação;
* desabilitar verificações para fazer um teste passar.
