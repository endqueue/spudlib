# SpudLib

Hardware Abstraction Layer (HAL) — a
from-scratch, unified C interface over GPU, filesystem, memory, audio, and network primitives.

## Overview

SpudLib is **pure mechanism, zero policy**: it translates a platform/GPU API
into one consistent C interface and stops there. It never makes a decision on
the caller's behalf, never has a "convenient" default that hides a choice, and holds no global state. All program logic lives one layer up; SpudLib only exposes enumeration/query functions so the caller can inspect and decide for itself.

```
The caller                    ← every decision
        ↓ C-ABI
SpudLib (HAL)                 ← you are here
        ↓
Native platform                ← Vulkan / D3D12 / Metal, Win32 / Linux / macOS
```

## Module map

Every module reports errors through the shared `SPUDRESULT` enum
(`spudcore.h`) rather than inventing its own convention.

| Module | Header | Prefix | Backends |
|---|---|---|---|
| SpudGPU | `spudgpu.h` | `spudgpu_` | Vulkan, D3D12, Metal |
| SpudFiles | `spudfiles.h` | `sfs_` | Windows only |
| SpudMemory | `spudmemory.h` | `smem_` | Windows, Linux |
| SpudNet | `spudnet.h` | `spudnet_` | Windows, Linux |
| SpudCore | `spudcore.h` | `spud_` / `SPUDRESULT` | platform-agnostic |
| SpudPerf | `spudperf.h` | `spudperf_` | Windows, Linux |

SpudGPU is the largest module by far (92 functions across 20 opaque handles).
The rest are small, focused translation shims by design — e.g. SpudNet moves
bytes, it doesn't do message framing or host authority; SpudMemory is
reserve/commit/decommit plus an arena allocator, not a general allocator
library.

## Backend status

| Backend | Status |
|---|---|
| Vulkan | Reference backend, plain C23, complete |
| D3D12 | Complete, C++26; cross-compiles SPIR-V to HLSL via SPIRV-Cross |
| Metal | Completed, Objective-C |

## Building

Each backend is selected via the `GRAPHICS_BACKEND` CMake cache variable
(`Vulkan` / `D3D12` / `Metal`) — there is no multi-backend build. Use the
CMake presets in `CMakePresets.json`, or build as part of the `eqdev`
workspace root to inherit the shared `CMAKE_MSVC_RUNTIME_LIBRARY` setting.
This avoids the need for a vtable which can destroy CPU branch predictions. VTables are an unecessary layer if the backend does not change per runtime.

```sh
cmake --preset <preset-name>
cmake --build --preset <preset-name>
```

> **Note:** `CMakePresets.json` currently has an unresolved merge conflict
> in the tracked file — resolve that before relying on the presets above.

## Known gaps

- No automated test suite/CTest target — verification has been manual
  smoke-testing.
- Metal backend is incomplete, untested.

## Contributing

See [`CLAUDE.md`](CLAUDE.md) for the design rules and conventions this repo
follows (the zero-policy principle, `SPUDGPU_EXT_*`/`SPUDGPU_LEGACY_*` gating,
backend file layout, etc.) before making structural changes. Run
`.clang-format` before committing.
