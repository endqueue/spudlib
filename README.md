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
| SpudFiles | `spudfiles.h` | `sfs_` | Windows, Linux, macOS |
| SpudMemory | `spudmemory.h` | `smem_` | Windows, POSIX (Linux + Apple, one file) |
| SpudNet | `spudnet.h` | `spudnet_` | TCP sockets: Winsock2, BSD sockets (Linux + Apple, one file). HTTP + WebSocket: WinHTTP, libcurl, NSURLSession |
| SpudAudio | `spudaudio.h` | `spudaudio_` / `SPUDRESULT_SAUD_*` | WASAPI (Windows), ALSA (Linux), CoreAudio (macOS); an argument-checking stub where no API is compiled in (iOS, watchOS) |
| SpudInput | `spudinput.h` | `spudinput_` / `SPUDRESULT_SPUDINPUT_*` | GameController (macOS, iOS, iPadOS), evdev (Linux), GameInput (Windows). Not written: XInput (Windows); with no API built in, no instance can be made |
| SpudCore | `spudcore.h` | `spud_` / `SPUDRESULT` | platform-agnostic |
| SpudPerf | `spudperf.h` | `spudperf_` | Windows, Linux, Apple |

SpudGPU is the largest module by far (133 functions across 24 opaque handles).
The rest are small, focused translation shims by design — e.g. SpudNet moves
bytes, it doesn't do message framing or host authority; SpudMemory is
reserve/commit/decommit/release of whole pages, not an allocator.

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

## Known gaps

- Eight CTest targets: `spudcore_test`, `spudgpu_device_test`, `spudgpu_objects_test`, `spudfiles_test`,
  `spudmemory_test`, `spudaudio_test`, `spudperf_test` and `spudinput_test`. The rest of SpudGPU
  is verified by running the `spudgpusamples` samples; see `CLAUDE.md`'s
  "Known gaps" for what each test covers and what has run where.

## Contributing

See [`CLAUDE.md`](CLAUDE.md) for the design rules and conventions this repo
follows (the zero-policy principle, `SPUDGPU_EXT_*` gating, with `SPUDGPU_LEGACY_*`
designed but not built,
backend file layout, etc.) before making structural changes. Run
`.clang-format` before committing.
