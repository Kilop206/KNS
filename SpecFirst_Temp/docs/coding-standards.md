# Padrões de Código — KNS

## 1. Linguagem

O KNS utiliza:

```text
C++20
```

O código deve permanecer compatível com o padrão C++20 configurado pelo projeto.

O CMake atual utiliza:

```text
CMAKE_CXX_STANDARD 20
CMAKE_CXX_STANDARD_REQUIRED ON
CMAKE_CXX_EXTENSIONS OFF
```

Portanto, extensões específicas do compilador não devem ser necessárias para o funcionamento normal do código.

---

## 2. Build

O sistema oficial é:

```text
CMake
```

No Windows, o ambiente de desenvolvimento definido para o projeto utiliza:

```text
C:\mingw64
```

MSYS2/UCRT64 não deve ser misturado com o toolchain MinGW utilizado pelo projeto.

---

## 3. Organização

### Headers

Declarações devem ficar em:

```text
core/include/
app/include/
```

### Implementações

Implementações devem ficar em:

```text
core/src/
app/src/
```

quando aplicável à estrutura do módulo.

### Testes

Testes devem permanecer em:

```text
tests/
```

---

## 4. Responsabilidade das classes

Cada classe deve possuir responsabilidade clara.

Evitar classes que simultaneamente:

* gerenciem GUI;
* implementem lógica de rede;
* controlem persistência;
* executem regras de protocolo;
* façam logging operacional.

Quando responsabilidades diferentes aparecem juntas, avaliar se a separação reduz acoplamento real.

---

## 5. Interfaces públicas

Interfaces públicas devem:

* possuir nomes claros;
* possuir contratos previsíveis;
* definir comportamento para entradas inválidas;
* evitar efeitos colaterais inesperados;
* ser consistentes entre overloads.

Mudanças de API devem ser avaliadas quanto ao impacto nos chamadores e testes existentes.

---

## 6. Validação

Entradas provenientes de:

* arquivos;
* CLI;
* GUI;
* topologia;
* APIs públicas;

devem ser validadas na camada apropriada.

Não depender apenas do `TopologyLoader` para garantir invariantes se a própria classe `Topology` puder ser utilizada diretamente.

---

## 7. Erros

Erros relevantes não devem ser silenciosamente convertidos em sucesso.

Por exemplo:

```text
transmissão rejeitada
≠
transmissão aceita
```

Uma API deve fornecer informação suficiente para que o chamador trate corretamente o resultado.

---

## 8. Memória

Preferir RAII e gerenciamento automático de recursos.

Para objetos com ownership exclusivo, utilizar estruturas apropriadas como:

```cpp
std::unique_ptr
```

Evitar `new`/`delete` manuais quando uma abstração RAII puder representar claramente a propriedade.

Eventos do KNS são armazenados utilizando ownership explícito.

---

## 9. Const-correctness

Usar `const` quando a função não altera o objeto.

Overloads const e não-const devem possuir contratos semanticamente consistentes.

Diferenças de comportamento entre os overloads devem ser justificadas e documentadas.

---

## 10. Determinismo

Código do `core` não deve depender de:

* timing da GUI;
* ordem acidental de containers;
* estado global não controlado;
* comportamento não determinístico sem justificativa.

Quando uma ordem específica for parte do comportamento, ela deve ser representada explicitamente.

---

## 11. Containers

Escolher containers de acordo com a semântica necessária.

Exemplos:

* `std::vector` para armazenamento contíguo/indexado;
* `std::deque` quando operações nas extremidades forem necessárias;
* `std::map` quando ordenação por chave for relevante;
* `std::unordered_map` quando apenas lookup por chave for necessário e ordenação não fizer parte do contrato;
* `std::queue`/`std::priority_queue` quando a política de acesso corresponder diretamente ao domínio.

Não substituir um container por outro apenas por preferência estética.

---

## 12. Filas e ordem

Quando uma fila representar um comportamento de rede, sua estrutura deve refletir o contrato.

No caso de uma fila FIFO:

```text
first in
   ↓
first out
```

Não utilizar apenas contadores quando o contrato exigir preservação da ordem ou identidade dos elementos.

---

## 13. Comentários

Comentários devem explicar:

* motivo;
* trade-off;
* decisão;
* comportamento não óbvio.

Evitar comentários que apenas repitam o código.

Preferir documentação arquitetural para decisões que afetem múltiplos módulos.

---

## 14. Nomenclatura

Priorizar nomes que expressem:

* intenção;
* domínio;
* estado;
* direção;
* unidade.

Evitar abreviações desnecessárias.

Quando unidades forem importantes, preservá-las no nome ou na documentação:

```text
delay_ms
bandwidth_mbps
packet_size_bytes
```

---

## 15. Implementação de bugs

Um bug deve, sempre que possível, ser convertido em um teste de regressão antes ou junto da correção.

Evitar corrigir sintomas em uma camada quando a causa está claramente em outra.

---

## 16. Refatoração

Refatoração deve:

* preservar comportamento;
* reduzir complexidade real;
* reduzir duplicação real;
* melhorar clareza.

Não fazer refatoração ampla apenas porque o arquivo foi aberto durante um bugfix.

---

## 17. Dependências

Antes de adicionar uma biblioteca:

1. verificar se o C++20 ou uma dependência existente já resolve;
2. avaliar custo de build e manutenção;
3. justificar a necessidade;
4. consultar `docs/decision-log.md` quando a decisão for estrutural.

---

## 18. Checklist de revisão

Antes da entrega:

* [ ] responsabilidade da mudança está clara;
* [ ] nenhuma dependência proibida foi criada;
* [ ] entradas inválidas são tratadas;
* [ ] ownership é claro;
* [ ] determinismo foi preservado;
* [ ] testes foram criados/atualizados;
* [ ] build passa;
* [ ] checks relevantes passam;
* [ ] documentação está sincronizada;
* [ ] não foi introduzido escopo não autorizado.
