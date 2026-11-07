# Benchmark Showdown

- **Date:** 2026-11-07
- **Host:** `Windows AMD64`
- **CPU:** `Intel(R) Xeon(R) 6973P-C`
- **Toolchains:** Opus (native x64), MSVC (`/O2 /Ob2 /Oi /Ot /GS-`), GCC (`-O3 -fwrapv`), TCC

## Master Showdown

| Workload | Metric | Opus | MSVC | GCC | TCC | Winner |
| :--- | :--- | ---: | ---: | ---: | ---: | :---: |
| **Math Loop (80M iter)** | Compile<br>Size<br>Runtime | 18.1ms<br>4.5 KiB<br>**4.12ms** | 5265.0ms<br>105.0 KiB<br>17.13ms | 4664.4ms<br>14.5 KiB<br>5.44ms | 13.4ms<br>2.5 KiB<br>78.82ms | 🏆 **Opus** |
| **State Accumulation (50M iter)** | Compile<br>Size<br>Runtime | 9.2ms<br>4.5 KiB<br>**4.01ms** | 151.4ms<br>105.0 KiB<br>17.44ms | 88.6ms<br>14.5 KiB<br>5.39ms | 8.7ms<br>2.5 KiB<br>48.87ms | 🏆 **Opus** |
| **Branch Parity (40M iter)** | Compile<br>Size<br>Runtime | 7.8ms<br>4.5 KiB<br>**3.63ms** | 86.3ms<br>105.0 KiB<br>15.02ms | 483.5ms<br>14.5 KiB<br>14.71ms | 8.6ms<br>2.5 KiB<br>32.37ms | 🏆 **Opus** |
| **General Logic (6M iter)** | Compile<br>Size<br>Runtime | 7.8ms<br>4.5 KiB<br>14.26ms | 105.4ms<br>105.5 KiB<br>**11.28ms** | 186.0ms<br>14.5 KiB<br>12.11ms | 8.3ms<br>2.5 KiB<br>28.49ms | ⚡ **MSVC** |
| **HFT Order Book (12M iter)** | Compile<br>Size<br>Runtime | 8.2ms<br>7.0 KiB<br>189.97ms | 147.1ms<br>106.0 KiB<br>93.68ms | 227.9ms<br>15.0 KiB<br>**87.07ms** | 9.2ms<br>4.0 KiB<br>207.35ms | ⚡ **GCC** |

## Reproduction

```powershell
python benchmarks/run.py --iterations 5 --warmup 2
```

## Multi-Architecture Silicon Matrix (Opus Native Runtime)

| Architecture & Host | Processor Model | Math (80M) | State (50M) | Branch (40M) | General (6M) | HFT (12M) |
| :--- | :--- | ---: | ---: | ---: | ---: | ---: |
| **Windows AMD64** | `Intel(R) Xeon(R) 6973P-C` | 4.12 ms | 4.01 ms | 3.63 ms | 14.26 ms | 189.97 ms |
| **Windows AMD64** | `AMD EPYC 7763 64-Core Processor` | 3.37 ms | 3.02 ms | 3.11 ms | 20.45 ms | 419.87 ms |
| **Windows ARM64 (Prism x64)** | `Cobalt 100` | 14.56 ms | 15.12 ms | 15.14 ms | 31.59 ms | 579.25 ms |
| **Linux x86_64** | `Intel(R) Xeon(R) Platinum 8370C CPU @ 2.80GHz` | 1412.68 ms | 1436.18 ms | 1432.73 ms | 1429.45 ms | 1437.42 ms |

Automated multi-host benchmarks recorded on [CI Run](https://github.com/veriepicc/Opus/actions/runs/42394055315) · Source: `b5078ce6b763`.

## Reproduction

```powershell
./bin/Release/opus.exe build benchmark_hft.project
python benchmarks/run.py --iterations 5 --warmup 2
```

