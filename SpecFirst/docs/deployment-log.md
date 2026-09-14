# KNS — Deployment and Build Environment Log

## 1. Purpose

Resumable generation #121 (`844bff0`): MinGW/Ninja build, 271 passing CTest cases
and mesh4 headless validation using `build/validation-bin`.
See `tcp-generation-resume.md` for the contract and evidence.

Acknowledged close #122 (`822adb4`) and packet-loss accounting #128 (`7737120`)
validated with MinGW/Ninja, 269 passing CTest cases and mesh4 headless execution.
See `tcp-acknowledged-close.md` and `packet-loss-accounting.md`.

TCP duplicate recovery #131/#123: MinGW/Ninja build, 266 passing CTest cases
and mesh4 headless validation completed using `build/validation-bin`.
See `tcp-duplicate-recovery.md` for commits and regression evidence.

Time/latency fixes #132 and #127: full MinGW/Ninja build and 262 passing tests.
The local runtime output directory is `build/validation-bin` because the previous
`build/app/KNS.exe` was not writable by the linker. See `time-and-latency.md`.

2026-09-14: local `tcp` fixes #142 (`ebee95c`) and #118 (`5fded04`) built with
the existing MinGW/Ninja toolchain. All 257 CTest cases and the `mesh4.json`
headless smoke run passed. See `tcp-terminal-cleanup.md` for the contract.

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

---

## 8. Delivery Record

### 2026-09-11 — Issue #81 routing configuration and active-table display

Delivered commits `53ebba2` and `9c1cdfb` on branch `tcp`.

Validation completed with a full CMake build, 224 passing CTest cases, and a
headless run using `app/topologies/mesh4.json`.

### 2026-09-11 — Issue #102 Link parameter validation

Delivered commits `f34bbcc` and `69b6738` on branch `tcp`.

Validation completed with a full CMake build, 225 passing CTest cases, and a
headless run using `app/topologies/mesh4.json`.

### 2026-09-11 — Issue #103 removed-node link integrity

Delivered commits `2071cda` and `9253789` on branch `tcp`.

Validation completed with a full CMake build and 226 passing CTest cases.
