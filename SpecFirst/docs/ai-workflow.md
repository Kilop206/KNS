# Fluxo de Trabalho com IA — KNS

## 1. Princípio

O desenvolvimento do KNS utiliza o seguinte modelo:

```text
Humano = navegador
IA     = piloto
```

O humano define:

* o que deve ser feito;
* por que deve ser feito;
* prioridade;
* escopo;
* decisões importantes.

A IA é responsável por propor e executar a solução técnica dentro desse escopo.

---

## 2. Regra principal

A IA não deve implementar uma mudança relevante sem primeiro compreender:

* objetivo;
* estado atual;
* documentação;
* testes;
* critério de aceitação;
* riscos.

---

## 3. Antes da implementação

Para uma tarefa relevante:

### 1. Ler o contrato

Ler:

```text
SpecFirst_Temp/AGENTS.md
```

### 2. Ler o contexto

Consultar os documentos relevantes.

### 3. Verificar o estado real

Quando a tarefa depender do estado atual do KNS:

* consultar a branch `tcp`;
* verificar a implementação atual;
* não assumir equivalência com código local.

### 4. Verificar a issue

Determinar se a issue continua válida.

### 5. Verificar os testes

Pesquisar testes existentes antes de criar novos.

---

## 4. Diagnóstico de issue

Toda issue deve seguir:

```text
Issue
 ↓
Código atual
 ↓
Documentação
 ↓
Testes
 ↓
Diagnóstico
 ↓
Classificação
```

Classificações:

* resolvida;
* parcialmente resolvida;
* ainda válida;
* obsoleta.

Não implementar uma issue somente por ela estar aberta.

---

## 5. Red-Green-Refactor

Para comportamento novo ou bug:

```text
RED
 ↓
teste reproduz o comportamento esperado
 ↓
GREEN
 ↓
implementação mínima
 ↓
REFACTOR
 ↓
melhoria estrutural sem mudança de comportamento
```

Refatoração ampla não deve ser introduzida automaticamente durante um bugfix.

---

## 6. Menor incremento seguro

Preferir:

```text
uma mudança pequena
+
um contrato claro
+
um teste
+
uma validação
```

em vez de:

```text
um bug
↓
reescrita de várias camadas
```

Quando uma solução exigir mudança arquitetural, isso deve ser apresentado como decisão separada.

---

## 7. Quando pedir decisão humana

A IA deve parar e solicitar decisão quando a tarefa exigir:

* expansão de escopo;
* alteração arquitetural relevante;
* novo módulo principal;
* mudança importante do protocolo;
* mudança de contrato público;
* mudança do modelo de dados;
* troca de toolchain;
* nova dependência importante;
* remoção de documento;
* reordenação significativa do plano.

---

## 8. Implementação

Durante a implementação:

1. alterar apenas os arquivos necessários;
2. preservar as interfaces existentes quando possível;
3. evitar duplicação;
4. não esconder falhas;
5. manter o determinismo;
6. adicionar testes;
7. validar os efeitos colaterais.

---

## 9. Documentação durante a implementação

Atualizar documentação quando:

* uma regra mudou;
* uma interface mudou;
* uma arquitetura mudou;
* o comportamento de um componente mudou;
* uma issue foi concluída;
* uma decisão duradoura foi tomada.

---

## 10. Fechamento

Antes de considerar uma tarefa concluída:

```text
Código
 ↓
Testes
 ↓
Checks
 ↓
Issue
 ↓
Plano
 ↓
Deployment Log
 ↓
Decision Log, se aplicável
```

O chat deve relatar:

* arquivos modificados;
* comportamento implementado;
* testes executados;
* resultado dos checks;
* documentação atualizada;
* riscos residuais.

---

## 11. Proibições

A IA não deve:

* inventar requisitos;
* inventar comportamento sem declarar a suposição;
* mudar escopo silenciosamente;
* declarar issue resolvida sem verificar o código;
* fechar uma tarefa sem testes quando testes forem aplicáveis;
* esconder falhas de build;
* misturar toolchains;
* introduzir dependências desnecessárias;
* substituir a arquitetura por conveniência local.

---

## 12. Contexto parcial

Quando nem todo o repositório precisar ser lido, a IA deve buscar apenas o contexto necessário.

Exemplos:

```text
Mudança em Link
→ Link.hpp
→ Link.cpp
→ SimulationEngine
→ PacketUtils
→ testes de Link/transmissão
```

```text
Mudança TCP
→ TCPConnection
→ TCPSession
→ eventos relevantes
→ protocol_spec.md
→ tcp_design.md
→ testes TCP
```

O objetivo é manter contexto suficiente sem gerar leitura indiscriminada do repositório.

---

## 13. Estado versus intenção

Um documento pode descrever uma intenção futura.

O código descreve o comportamento efetivamente implementado.

A IA deve distinguir:

```text
"planejado"
```

de:

```text
"implementado"
```

e:

```text
"parcialmente implementado"
```

Essa distinção é especialmente importante para as issues do KNS.