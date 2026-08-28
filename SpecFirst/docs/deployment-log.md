# KNS — Deployment and Build Environment Log

## 1. Purpose

This document records relevant build and deployment environment decisions.

KNS is primarily a development and simulation application rather than a conventional server deployment.

---

## 2. Windows Toolchain

The intended Windows compiler environment is MinGW:

```text
C:\mingw64
```

The project must avoid mixing binaries and runtime libraries from unrelated toolchains.

---

## 3. CMake

KNS uses CMake as its build-system configuration layer.

The active compiler and runtime environment should be verified through the generated CMake configuration.

Do not assume that the selected compiler is the one implied by the system PATH.

---

## 4. Runtime Compatibility

Executable runtime failures may result from incompatible or missing runtime libraries.

A known class of failure occurred when the build was configured for MSYS2 UCRT64 while the environment prioritized another MinGW installation.

The correct approach is to configure and build consistently with the intended toolchain rather than relying on PATH workarounds.

---

## 5. Build Verification

After changing toolchains:

```text
Clean configuration
        ↓
CMake configure
        ↓
Build
        ↓
Run tests
        ↓
Run executable
```

The generated build configuration should be inspected before assuming the correct compiler was selected.

---

## 6. Deployment Artifacts

Deployment artifacts must correspond to the same compiler/runtime environment used during the build.

Do not distribute an executable together with DLLs originating from an incompatible toolchain.

---

## 7. Environment Changes

Significant changes to:

* compiler;
* CMake generator;
* runtime;
* dependency versions;

should be recorded when they affect reproducibility.
