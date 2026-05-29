# Benchmarks

This file is the benchmark proof page for the repo.

The goal is simple:

- keep the Opus benchmark sources in-tree
- keep the benchmark harness and recorded results in-tree
- record the exact comparison configuration
- avoid bloating the repo with heavyweight foreign benchmark ballast just to make the point louder

## What is in the repo

Opus benchmark sources:

- [tests/benchmark_math.op](tests/benchmark_math.op)
- [tests/benchmark_state.op](tests/benchmark_state.op)
- [tests/benchmark_branch.op](tests/benchmark_branch.op)
- [tests/benchmark_general.op](tests/benchmark_general.op)
- [tests/benchmark_text.op](tests/benchmark_text.op)
- [tests/benchmark_hft.op](tests/benchmark_hft.op)

Recorded results:

- [benchmark_results.csv](benchmark_results.csv)

## What this page contains

This page keeps the benchmark details in one place:

- exact benchmark names
- exact sample counts
- exact build mode
- exact MSVC settings
- exact recorded numbers

The README stays short; this page carries the full benchmark record.

## Harness settings

- measured iterations per executable: `8`
- warmup iterations per executable: `2`
- trim fraction: `0.1`
- results recorded:
  `CompileMs`, `AvgMs`, `TrimmedAvgMs`, `MedianMs`, `MinMs`, `MaxMs`, `P90Ms`, `P95Ms`, `StdDevMs`, `SizeBytes`, and `ExitCodes`

The harness builds Opus benchmarks with:

```text
.\bin\Release\opus.exe <benchmark.op>
```

and measures MSVC by rebuilding the matching benchmark project with:

```text
MSBuild.exe <benchmark>.vcxproj /t:Rebuild /p:Configuration=Release /p:Platform=x64 /m
```

## Exact MSVC benchmark configuration

The comparison projects are configured as **Release|x64**, not Debug.

Across the benchmark `.vcxproj` files inspected for this run, the relevant MSVC settings are:

- `ConfigurationType=Application`
- `PlatformToolset=v145`
- `UseDebugLibraries=false`
- `WholeProgramOptimization=true`
- `CharacterSet=Unicode`

Compiler settings:

- `WarningLevel=Level4`
- `Optimization=MaxSpeed`
- `FunctionLevelLinking=true`
- `IntrinsicFunctions=true`
- `SDLCheck=true`
- `PreprocessorDefinitions=NDEBUG;_CONSOLE;%(PreprocessorDefinitions)`
- `ConformanceMode=true`
- `LanguageStandard=stdcpplatest`
- `MultiProcessorCompilation=true`
- `FavorSizeOrSpeed=Speed`
- `AdditionalOptions=/utf-8 %(AdditionalOptions)`

Linker settings:

- `SubSystem=Console`
- `EnableCOMDATFolding=true`
- `OptimizeReferences=true`
- `GenerateDebugInformation=true`

That means the benchmark comparison is not secretly using Debug-mode MSVC.

## Result snapshot

Numbers below are taken directly from [benchmark_results.csv](benchmark_results.csv).

### Compile time

| Benchmark | Opus | MSVC |
|------|------:|------:|
| `benchmark_math` | `6.877ms` | `889.560ms` |
| `benchmark_state` | `8.558ms` | `1008.338ms` |
| `benchmark_branch` | `4.886ms` | `893.357ms` |
| `benchmark_general` | `5.384ms` | `946.994ms` |
| `benchmark_text` | `5.978ms` | `908.733ms` |
| `benchmark_hft` | `5.928ms` | `918.519ms` |

### Trimmed runtime average

| Benchmark | Opus | MSVC |
|------|------:|------:|
| `benchmark_math` | `2.400ms` | `25.174ms` |
| `benchmark_state` | `5.335ms` | `17.075ms` |
| `benchmark_branch` | `2.504ms` | `25.345ms` |
| `benchmark_general` | `28.531ms` | `32.734ms` |
| `benchmark_text` | `25.993ms` | `11.520ms` |
| `benchmark_hft` | `271.652ms` | `419.364ms` |

### Binary size

| Benchmark | Opus | MSVC |
|------|------:|------:|
| `benchmark_math` | `4096` bytes | `9728` bytes |
| `benchmark_state` | `4096` bytes | `9728` bytes |
| `benchmark_branch` | `4608` bytes | `9728` bytes |
| `benchmark_general` | `5120` bytes | `9728` bytes |
| `benchmark_text` | `6656` bytes | `12288` bytes |
| `benchmark_hft` | `7168` bytes | `11264` bytes |

## How to read this

These benchmarks are not trying to prove that Opus wins every workload forever.

They prove something narrower and more important:

- Opus compile times are dramatically lower on this benchmark set
- runtime is already competitive, and in several cases clearly ahead
- the generated binaries stay small

That is enough to justify taking the language seriously.

## Repo policy for benchmark artifacts

The committed repo should keep:

- Opus benchmark sources
- result snapshots
- the exact comparison configuration used

The committed repo should **not** need to carry a giant pile of comparison-side build junk just to make the proof auditable.

The proof is the config, the harness, the Opus sources, and the recorded numbers.
