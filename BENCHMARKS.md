# Benchmark Showdown

- **Date:** 2026-11-07
- **Host:** `Windows AMD64`
- **CPU:** `AMD EPYC 9V74 80-Core Processor`
- **Toolchains:** Opus (native x64), MSVC (`/O2 /Ob2 /Oi /Ot /GS-`), GCC (`-O3 -fwrapv`), TCC

## Master Showdown

| Workload | Metric | Opus | MSVC | GCC | TCC | Winner |
| :--- | :--- | ---: | ---: | ---: | ---: | :---: |
| **Math Loop (80M iter)** | Compile<br>Size<br>Runtime | 12.5ms<br>4.5 KiB<br>**3.34ms** | 3671.4ms<br>105.0 KiB<br>26.36ms | 3038.6ms<br>14.5 KiB<br>4.26ms | 12.1ms<br>2.5 KiB<br>114.77ms | **Opus** |
| **State Accumulation (50M iter)** | Compile<br>Size<br>Runtime | 7.2ms<br>4.5 KiB<br>**3.38ms** | 85.1ms<br>105.0 KiB<br>18.22ms | 77.8ms<br>14.5 KiB<br>4.39ms | 8.1ms<br>2.5 KiB<br>73.34ms | **Opus** |
| **Branch Parity (40M iter)** | Compile<br>Size<br>Runtime | 7.2ms<br>4.5 KiB<br>**3.34ms** | 82.7ms<br>105.0 KiB<br>26.60ms | 320.1ms<br>14.5 KiB<br>17.46ms | 8.0ms<br>2.5 KiB<br>39.44ms | **Opus** |
| **General Logic (6M iter)** | Compile<br>Size<br>Runtime | 7.6ms<br>4.5 KiB<br>16.74ms | 97.7ms<br>105.5 KiB<br>15.96ms | 138.1ms<br>14.5 KiB<br>**15.48ms** | 8.0ms<br>2.5 KiB<br>38.03ms | **GCC** |
| **HFT Order Book (12M iter)** | Compile<br>Size<br>Runtime | 7.9ms<br>7.0 KiB<br>219.36ms | 108.6ms<br>106.0 KiB<br>120.02ms | 170.4ms<br>15.0 KiB<br>**112.01ms** | 8.5ms<br>4.0 KiB<br>258.53ms | **GCC** |

## Reproduction

```powershell
python benchmarks/run.py --iterations 5 --warmup 2
```

## Multi-Architecture Silicon Matrix (Opus Native Runtime)

| Architecture & Host | Processor Model | Math (80M) | State (50M) | Branch (40M) | General (6M) | HFT (12M) |
| :--- | :--- | ---: | ---: | ---: | ---: | ---: |
| **Windows AMD64** | `AMD EPYC 9V74 80-Core Processor` | 3.34 ms | 3.38 ms | 3.34 ms | 16.74 ms | 219.36 ms |
| **Windows AMD64** | `AMD EPYC 9V45 96-Core Processor` | 2.02 ms | 1.94 ms | 2.04 ms | 11.29 ms | 140.55 ms |
| **Windows ARM64 (Prism x64)** | `Cobalt 100` | 14.92 ms | 13.82 ms | 14.90 ms | 31.60 ms | 579.10 ms |
| **Linux x86_64** | `AMD EPYC 7763 64-Core Processor` | 1489.56 ms | 1501.83 ms | 1485.15 ms | 1509.81 ms | 1502.80 ms |

Automated multi-host benchmarks recorded on [CI Run](https://github.com/veriepicc/Opus/actions/runs/42397081142) · Source: `9fb0749b24a1`.

## Reproduction

```powershell
./bin/Release/opus.exe build benchmark_hft.project
python benchmarks/run.py --iterations 5 --warmup 2
```

