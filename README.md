# GNS — Generic Network Simulator

## Overview

GNS é um simulador de redes orientado a eventos, com foco em determinismo, modularidade e base científica sólida.

O projeto está sendo desenvolvido incrementalmente, priorizando:

- Arquitetura limpa
- Separação clara de responsabilidades
- Testes automatizados
- Determinismo forte na simulação

---

## Objetivo

Construir um motor de simulação de redes baseado em eventos discretos que permita:

- Execução determinística
- Reprodutibilidade de resultados
- Extensibilidade modular
- Base sólida para futuras abstrações de rede

---

## Arquitetura Atual
	GNS/
	|-- core/ # Componentes principais do motor de simulação
	|-- app/ # Executável principal
	|-- tests/ # Testes unitários
	|-- docs/ # Documentação técnica
	|-- CMakeLists.txt

---

## Tecnologias Utilizadas

- C++20
- CMake
- Catch2 (testes unitários)
- CTest

---

## Como Compilar

### 1️º Gerar build

```bash
cmake -S . -B build
```

### 2️º Compilar
```bash
cmake --build build
```

Como o projeto usa gerador multi-config (Visual Studio):
### 3️º Executar testes
```bash
ctest -C Debug --output-on-failure
```

---

# GNS — Generic Network Simulator

## Overview

GNS é um simulador de redes orientado a eventos, com foco em determinismo, modularidade e base científica sólida.

O projeto está sendo desenvolvido incrementalmente, priorizando:

- Arquitetura limpa
- Separação clara de responsabilidades
- Testes automatizados
- Determinismo forte na simulação

---

## Objetivo

Construir um motor de simulação de redes baseado em eventos discretos que permita:

- Execução determinística
- Reprodutibilidade de resultados
- Extensibilidade modular
- Base sólida para futuras abstrações de rede

---

## Arquitetura Atual
GNS/
├── core/ # Componentes principais do motor de simulação
├── app/ # Executável principal
├── tests/ # Testes unitários
├── docs/ # Documentação técnica
└── CMakeLists.txt


---

## Tecnologias Utilizadas

- C++ (MSVC / padrão moderno)
- CMake
- Catch2 (testes unitários)
- CTest

---

## Como Compilar

### 1️º Gerar build

```bash
cmake -S . -B build
```

### 2️º Compilar
```bash
cmake --build build
```

Como o projeto usa gerador multi-config (Visual Studio):
### 3️º Executar testes
```bash
cteste -C Debug --output-on-failure
```

## Documentação Técnica

As decisões arquiteturais e a modelagem do motor de eventos estão descritas em:

`core/event_engine_design.md`

---

## Status do Projeto

Versão atual: v0.1 (Fundação)

- Estrutura modular estabelecida
- Clock determinístico implementado
- Pipeline de testes funcional
- Início do motor de eventos (em desenvolvimento)