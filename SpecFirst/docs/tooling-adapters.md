# KNS — Tooling and Adapters

## 1. Purpose

This document describes the role of development and validation tooling around KNS.

---

## 2. CMake

CMake is responsible for configuring the build system and expressing project build requirements.

The generated configuration must use the intended compiler environment.

---

## 3. CTest

CTest provides test execution and integration with the project's test infrastructure.

Tests should be registered so that relevant test cases can be executed independently when supported.

---

## 4. Catch2

Catch2 is used for automated C++ testing where configured by the repository.

Test discovery may use:

```text
catch_discover_tests
```

The exact configuration remains defined by the repository.

---

## 5. Headless Runner

Headless execution provides a way to validate core simulation behavior without the GUI.

It should be preferred for automated scenarios that do not require rendering.

---

## 6. GUI Tooling

The GUI uses:

```text
Dear ImGui
GLFW
OpenGL
```

These dependencies belong to the presentation layer.

Core simulation behavior should remain testable independently.

---

## 7. JSON / Topology Data

Topology data is loaded from external representations.

The loader is responsible for validating input before creating runtime topology state.

---

## 8. Toolchain Consistency

Development tooling must use a consistent compiler/runtime environment.

For the intended Windows configuration:

```text
C:\mingw64
```

is the target MinGW installation.

---

## 9. Tooling Changes

When adding or replacing tooling, evaluate:

* reproducibility;
* build integration;
* CI compatibility;
* developer experience;
* dependency impact;
* test integration.

Avoid introducing tooling solely to solve a local environment problem.
