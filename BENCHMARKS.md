# Multi-Platform Benchmark Results

Automated multi-host benchmarks recorded on [CI Run](https://github.com/veriepicc/Opus/actions/runs/42392835706) · Source: `f43be582229d`.


---

## Platform: Windows AMD64 (AMD EPYC 9V45 96-Core Processor)

# Benchmark Results

- Date: 2026-11-07
- Host: `Windows AMD64 (AMD EPYC 9V45 96-Core Processor)`
- Toolchains: Opus, GCC, MSVC, TCC
- Settings: GCC `-O3 -fwrapv`, MSVC `/O2`, TCC default optimization; Opus native x64.

## Summary

| Benchmark | Best Runtime | Fastest Compile | Smallest Binary |
| :--- | :--- | :--- | :--- |
| Math Loop (80M iter) | **Opus** (2.86ms) | **TCC** (10.3ms) | **TCC** (2560B) |
| State Accumulation (50M iter) | **Opus** (2.86ms) | **Opus** (6.7ms) | **TCC** (2560B) |
| Branch Parity (40M iter) | **Opus** (2.94ms) | **Opus** (7.1ms) | **TCC** (2560B) |
| General Logic (6M iter) | **MSVC** (12.59ms) | **Opus** (7.4ms) | **TCC** (2560B) |
| HFT Order Book (12M iter) | **GCC** (87.26ms) | **Opus** (8.0ms) | **TCC** (4096B) |

## Detailed Measurements

### Math Loop (80M iter)
*Power-of-2 shifts, adds, and induction operations*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 10.8 | 4,608 B | 2.86 | 3.27 | **Fastest** |
| **GCC** | 3087.0 | 14,848 B | 3.85 | 3.96 | 1.34x baseline |
| **MSVC** | 3358.6 | 107,520 B | 22.87 | 23.01 | 7.98x baseline |
| **TCC** | 10.3 | 2,560 B | 79.95 | 84.30 | 27.91x baseline |

### State Accumulation (50M iter)
*Multiple state updates with scalar induction*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 6.7 | 4,608 B | 2.86 | 2.92 | **Fastest** |
| **GCC** | 66.6 | 14,848 B | 3.76 | 3.80 | 1.32x baseline |
| **MSVC** | 74.6 | 107,520 B | 15.54 | 15.78 | 5.44x baseline |
| **TCC** | 7.3 | 2,560 B | 50.99 | 51.81 | 17.86x baseline |

### Branch Parity (40M iter)
*Alternating parity branch recurrence*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 7.1 | 4,608 B | 2.94 | 3.00 | **Fastest** |
| **GCC** | 314.8 | 14,848 B | 14.14 | 14.53 | 4.80x baseline |
| **MSVC** | 78.2 | 107,520 B | 15.78 | 15.94 | 5.36x baseline |
| **TCC** | 8.6 | 2,560 B | 31.66 | 33.10 | 10.75x baseline |

### General Logic (6M iter)
*Control flow folding, conditionals, and math blend*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 7.4 | 4,608 B | 12.79 | 12.94 | 1.02x baseline |
| **GCC** | 131.1 | 14,848 B | 12.66 | 12.73 | 1.01x baseline |
| **MSVC** | 90.1 | 108,032 B | 12.59 | 13.18 | **Fastest** |
| **TCC** | 7.7 | 2,560 B | 26.72 | 26.96 | 2.12x baseline |

### HFT Order Book (12M iter)
*L2 order book matching, inventory skew, and risk checksums*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 8.0 | 7,168 B | 153.98 | 156.90 | 1.76x baseline |
| **GCC** | 153.9 | 15,360 B | 87.26 | 88.22 | **Fastest** |
| **MSVC** | 156.9 | 108,544 B | 95.43 | 96.83 | 1.09x baseline |
| **TCC** | 8.3 | 4,096 B | 172.77 | 175.41 | 1.98x baseline |

## Reproduction

```bash
python benchmarks/run.py
```


---

## Platform: Windows AMD64 (AMD EPYC 7763 64-Core Processor)

# Benchmark Results

- Date: 2026-11-07
- Host: `Windows AMD64 (AMD EPYC 7763 64-Core Processor)`
- Toolchains: Opus, GCC, MSVC, TCC
- Settings: GCC `-O3 -fwrapv`, MSVC `/O2`, TCC default optimization; Opus native x64.

## Summary

| Benchmark | Best Runtime | Fastest Compile | Smallest Binary |
| :--- | :--- | :--- | :--- |
| Math Loop (80M iter) | **Opus** (3.31ms) | **TCC** (12.0ms) | **TCC** (2560B) |
| State Accumulation (50M iter) | **Opus** (3.47ms) | **Opus** (8.2ms) | **TCC** (2560B) |
| Branch Parity (40M iter) | **Opus** (3.63ms) | **Opus** (7.7ms) | **TCC** (2560B) |
| General Logic (6M iter) | **GCC** (18.17ms) | **Opus** (8.7ms) | **TCC** (2560B) |
| HFT Order Book (12M iter) | **MSVC** (127.85ms) | **TCC** (8.7ms) | **TCC** (4096B) |

## Detailed Measurements

### Math Loop (80M iter)
*Power-of-2 shifts, adds, and induction operations*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 16.0 | 4,608 B | 3.31 | 3.38 | **Fastest** |
| **GCC** | 3013.4 | 14,848 B | 4.68 | 4.79 | 1.41x baseline |
| **MSVC** | 4066.4 | 108,032 B | 30.34 | 30.38 | 9.16x baseline |
| **TCC** | 12.0 | 2,560 B | 128.98 | 131.19 | 38.95x baseline |

### State Accumulation (50M iter)
*Multiple state updates with scalar induction*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 8.2 | 4,608 B | 3.47 | 3.58 | **Fastest** |
| **GCC** | 98.4 | 14,848 B | 5.52 | 5.70 | 1.59x baseline |
| **MSVC** | 105.9 | 108,032 B | 23.12 | 23.28 | 6.67x baseline |
| **TCC** | 9.3 | 2,560 B | 82.61 | 82.91 | 23.84x baseline |

### Branch Parity (40M iter)
*Alternating parity branch recurrence*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 7.7 | 4,608 B | 3.63 | 3.72 | **Fastest** |
| **GCC** | 267.0 | 14,848 B | 20.01 | 20.13 | 5.52x baseline |
| **MSVC** | 105.0 | 108,032 B | 24.00 | 24.27 | 6.62x baseline |
| **TCC** | 9.1 | 2,560 B | 50.00 | 50.75 | 13.79x baseline |

### General Logic (6M iter)
*Control flow folding, conditionals, and math blend*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 8.7 | 4,608 B | 21.34 | 21.80 | 1.17x baseline |
| **GCC** | 157.3 | 15,360 B | 18.17 | 18.40 | **Fastest** |
| **MSVC** | 109.8 | 108,032 B | 38.87 | 39.07 | 2.14x baseline |
| **TCC** | 9.1 | 2,560 B | 47.01 | 47.42 | 2.59x baseline |

### HFT Order Book (12M iter)
*L2 order book matching, inventory skew, and risk checksums*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 9.5 | 7,168 B | 420.41 | 421.24 | 3.29x baseline |
| **GCC** | 204.0 | 15,872 B | 129.58 | 130.11 | 1.01x baseline |
| **MSVC** | 114.5 | 108,544 B | 127.85 | 128.29 | **Fastest** |
| **TCC** | 8.7 | 4,096 B | 395.63 | 397.17 | 3.09x baseline |

## Reproduction

```bash
python benchmarks/run.py
```


---

## Platform: Windows AMD64 (Cobalt 100)

# Benchmark Results

- Date: 2026-11-07
- Host: `Windows AMD64 (Cobalt 100)`
- Toolchains: Opus, GCC, TCC
- Settings: GCC `-O3 -fwrapv`, MSVC `/O2`, TCC default optimization; Opus native x64.

## Summary

| Benchmark | Best Runtime | Fastest Compile | Smallest Binary |
| :--- | :--- | :--- | :--- |
| Math Loop (80M iter) | **Opus** (14.16ms) | **TCC** (57.2ms) | **TCC** (2560B) |
| State Accumulation (50M iter) | **Opus** (15.07ms) | **Opus** (30.4ms) | **TCC** (2560B) |
| Branch Parity (40M iter) | **Opus** (14.37ms) | **Opus** (28.8ms) | **TCC** (2560B) |
| General Logic (6M iter) | **Opus** (30.77ms) | **TCC** (36.0ms) | **TCC** (2560B) |
| HFT Order Book (12M iter) | **GCC** (147.89ms) | **Opus** (32.7ms) | **TCC** (4096B) |

## Detailed Measurements

### Math Loop (80M iter)
*Power-of-2 shifts, adds, and induction operations*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 100.4 | 4,608 B | 14.16 | 14.41 | **Fastest** |
| **GCC** | 4122.5 | 14,848 B | 17.63 | 18.13 | 1.25x baseline |
| **TCC** | 57.2 | 2,560 B | 195.24 | 195.63 | 13.79x baseline |

### State Accumulation (50M iter)
*Multiple state updates with scalar induction*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 30.4 | 4,608 B | 15.07 | 15.41 | **Fastest** |
| **GCC** | 237.3 | 14,848 B | 17.28 | 17.63 | 1.15x baseline |
| **TCC** | 32.4 | 2,560 B | 195.61 | 195.94 | 12.98x baseline |

### Branch Parity (40M iter)
*Alternating parity branch recurrence*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 28.8 | 4,608 B | 14.37 | 14.69 | **Fastest** |
| **GCC** | 585.1 | 14,848 B | 36.33 | 36.91 | 2.53x baseline |
| **TCC** | 31.9 | 2,560 B | 103.56 | 104.06 | 7.21x baseline |

### General Logic (6M iter)
*Control flow folding, conditionals, and math blend*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 37.9 | 4,608 B | 30.77 | 31.09 | **Fastest** |
| **GCC** | 360.5 | 15,360 B | 31.43 | 31.98 | 1.02x baseline |
| **TCC** | 36.0 | 2,560 B | 88.53 | 88.74 | 2.88x baseline |

### HFT Order Book (12M iter)
*L2 order book matching, inventory skew, and risk checksums*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 32.7 | 7,168 B | 579.93 | 581.48 | 3.92x baseline |
| **GCC** | 439.3 | 15,872 B | 147.89 | 148.29 | **Fastest** |
| **TCC** | 33.3 | 4,096 B | 470.03 | 470.90 | 3.18x baseline |

## Reproduction

```bash
python benchmarks/run.py
```


---

## Platform: Linux x86_64 (Intel(R) Xeon(R) 6973P-C)

# Benchmark Results

- Date: 2026-11-07
- Host: `Linux x86_64 (Intel(R) Xeon(R) 6973P-C)`
- Toolchains: Opus, GCC, TCC
- Settings: GCC `-O3 -fwrapv`, MSVC `/O2`, TCC default optimization; Opus native x64.

## Summary

| Benchmark | Best Runtime | Fastest Compile | Smallest Binary |
| :--- | :--- | :--- | :--- |
| Math Loop (80M iter) | **Opus** (1408.46ms) | **GCC** (27.4ms) | **TCC** (2560B) |
| State Accumulation (50M iter) | **TCC** (1423.01ms) | **GCC** (30.1ms) | **TCC** (2560B) |
| Branch Parity (40M iter) | **Opus** (1428.46ms) | **GCC** (27.7ms) | **TCC** (2560B) |
| General Logic (6M iter) | **GCC** (1419.28ms) | **GCC** (30.7ms) | **TCC** (2560B) |
| HFT Order Book (12M iter) | **Opus** (1394.89ms) | **GCC** (43.7ms) | **TCC** (4096B) |

## Detailed Measurements

### Math Loop (80M iter)
*Power-of-2 shifts, adds, and induction operations*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 5533.0 | 4,608 B | 1408.46 | 1449.77 | **Fastest** |
| **GCC** | 27.4 | 14,848 B | 1433.52 | 1454.20 | 1.02x baseline |
| **TCC** | 1465.6 | 2,560 B | 1434.26 | 1529.60 | 1.02x baseline |

### State Accumulation (50M iter)
*Multiple state updates with scalar induction*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 3426.9 | 4,608 B | 1447.23 | 1464.54 | 1.02x baseline |
| **GCC** | 30.1 | 14,848 B | 1428.81 | 1453.85 | 1.00x baseline |
| **TCC** | 1421.8 | 2,560 B | 1423.01 | 1462.71 | **Fastest** |

### Branch Parity (40M iter)
*Alternating parity branch recurrence*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 1465.7 | 4,608 B | 1428.46 | 1465.47 | **Fastest** |
| **GCC** | 27.7 | 14,848 B | 1429.30 | 1437.00 | 1.00x baseline |
| **TCC** | 1462.1 | 2,560 B | 1441.46 | 1455.25 | 1.01x baseline |

### General Logic (6M iter)
*Control flow folding, conditionals, and math blend*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 1449.0 | 4,608 B | 1430.90 | 1445.26 | 1.01x baseline |
| **GCC** | 30.7 | 14,848 B | 1419.28 | 1462.85 | **Fastest** |
| **TCC** | 1512.7 | 2,560 B | 1428.53 | 1453.69 | 1.01x baseline |

### HFT Order Book (12M iter)
*L2 order book matching, inventory skew, and risk checksums*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 1479.8 | 7,168 B | 1394.89 | 1451.81 | **Fastest** |
| **GCC** | 43.7 | 15,872 B | 1444.28 | 1456.92 | 1.04x baseline |
| **TCC** | 1465.7 | 4,096 B | 1434.22 | 1462.97 | 1.03x baseline |

## Reproduction

```bash
python benchmarks/run.py
```

