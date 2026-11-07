# Multi-Platform Benchmark Results

Automated multi-host benchmarks recorded on [CI Run](https://github.com/veriepicc/Opus/actions/runs/42391885141) · Source: `6b6cd6aa8f24`.


---

## Platform: Linux x86_64 (AMD EPYC 9V74 80-Core Processor)

# Benchmark Results

- Date: 2026-11-07
- Host: `Linux x86_64 (AMD EPYC 9V74 80-Core Processor)`
- Toolchains: Opus, GCC, TCC
- Settings: GCC `-O3 -fwrapv`, MSVC `/O2`, TCC default optimization; Opus native x64.

## Summary

| Benchmark | Best Runtime | Fastest Compile | Smallest Binary |
| :--- | :--- | :--- | :--- |
| Math Loop (80M iter) | **GCC** (1389.53ms) | **GCC** (30.2ms) | **TCC** (2560B) |
| State Accumulation (50M iter) | **Opus** (1384.31ms) | **GCC** (28.9ms) | **TCC** (2560B) |
| Branch Parity (40M iter) | **Opus** (1383.05ms) | **GCC** (30.8ms) | **TCC** (2560B) |
| General Logic (6M iter) | **TCC** (1387.12ms) | **GCC** (34.8ms) | **TCC** (2560B) |
| HFT Order Book (12M iter) | **TCC** (1393.87ms) | **GCC** (48.5ms) | **TCC** (4096B) |

## Detailed Measurements

### Math Loop (80M iter)
*Power-of-2 shifts, adds, and induction operations*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 3460.6 | 4,608 B | 1400.78 | 1404.74 | 1.01x baseline |
| **GCC** | 30.2 | 14,848 B | 1389.53 | 1400.35 | **Fastest** |
| **TCC** | 1407.2 | 2,560 B | 1402.29 | 1412.84 | 1.01x baseline |

### State Accumulation (50M iter)
*Multiple state updates with scalar induction*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 1557.2 | 4,608 B | 1384.31 | 1404.26 | **Fastest** |
| **GCC** | 28.9 | 14,848 B | 1394.55 | 1414.67 | 1.01x baseline |
| **TCC** | 1415.6 | 2,560 B | 1406.17 | 1413.44 | 1.02x baseline |

### Branch Parity (40M iter)
*Alternating parity branch recurrence*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 1401.3 | 4,608 B | 1383.05 | 1399.25 | **Fastest** |
| **GCC** | 30.8 | 14,848 B | 1399.82 | 1405.51 | 1.01x baseline |
| **TCC** | 1401.7 | 2,560 B | 1400.37 | 1407.27 | 1.01x baseline |

### General Logic (6M iter)
*Control flow folding, conditionals, and math blend*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 1417.5 | 4,608 B | 1399.25 | 1406.13 | 1.01x baseline |
| **GCC** | 34.8 | 14,848 B | 1389.46 | 1401.23 | 1.00x baseline |
| **TCC** | 1408.0 | 2,560 B | 1387.12 | 1406.86 | **Fastest** |

### HFT Order Book (12M iter)
*L2 order book matching, inventory skew, and risk checksums*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 1390.2 | 7,168 B | 1394.87 | 1805.88 | 1.00x baseline |
| **GCC** | 48.5 | 15,872 B | 1397.87 | 1405.14 | 1.00x baseline |
| **TCC** | 1405.4 | 4,096 B | 1393.87 | 1404.71 | **Fastest** |

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
| Math Loop (80M iter) | **Opus** (3.04ms) | **TCC** (10.9ms) | **TCC** (2560B) |
| State Accumulation (50M iter) | **Opus** (3.01ms) | **Opus** (6.9ms) | **TCC** (2560B) |
| Branch Parity (40M iter) | **Opus** (3.14ms) | **Opus** (7.3ms) | **TCC** (2560B) |
| General Logic (6M iter) | **GCC** (16.38ms) | **Opus** (8.0ms) | **TCC** (2560B) |
| HFT Order Book (12M iter) | **MSVC** (127.87ms) | **TCC** (8.7ms) | **TCC** (4096B) |

## Detailed Measurements

### Math Loop (80M iter)
*Power-of-2 shifts, adds, and induction operations*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 15.9 | 4,608 B | 3.04 | 3.10 | **Fastest** |
| **GCC** | 2971.4 | 14,848 B | 4.41 | 4.48 | 1.45x baseline |
| **MSVC** | 3996.6 | 108,032 B | 28.80 | 29.00 | 9.47x baseline |
| **TCC** | 10.9 | 2,560 B | 128.11 | 128.72 | 42.11x baseline |

### State Accumulation (50M iter)
*Multiple state updates with scalar induction*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 6.9 | 4,608 B | 3.01 | 3.07 | **Fastest** |
| **GCC** | 84.1 | 14,848 B | 4.09 | 4.16 | 1.36x baseline |
| **MSVC** | 90.2 | 108,032 B | 22.48 | 22.63 | 7.48x baseline |
| **TCC** | 8.3 | 2,560 B | 82.22 | 82.46 | 27.34x baseline |

### Branch Parity (40M iter)
*Alternating parity branch recurrence*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 7.3 | 4,608 B | 3.14 | 3.26 | **Fastest** |
| **GCC** | 248.1 | 14,848 B | 18.59 | 18.88 | 5.93x baseline |
| **MSVC** | 92.1 | 108,032 B | 23.18 | 23.49 | 7.39x baseline |
| **TCC** | 8.7 | 2,560 B | 49.31 | 49.59 | 15.73x baseline |

### General Logic (6M iter)
*Control flow folding, conditionals, and math blend*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 8.0 | 4,608 B | 20.45 | 20.54 | 1.25x baseline |
| **GCC** | 155.2 | 15,360 B | 16.38 | 16.54 | **Fastest** |
| **MSVC** | 96.2 | 108,032 B | 38.10 | 38.43 | 2.33x baseline |
| **TCC** | 8.2 | 2,560 B | 46.34 | 47.07 | 2.83x baseline |

### HFT Order Book (12M iter)
*L2 order book matching, inventory skew, and risk checksums*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 8.9 | 7,168 B | 419.94 | 420.69 | 3.28x baseline |
| **GCC** | 184.3 | 15,872 B | 128.79 | 129.05 | 1.01x baseline |
| **MSVC** | 102.3 | 108,544 B | 127.87 | 130.29 | **Fastest** |
| **TCC** | 8.7 | 4,096 B | 394.18 | 402.51 | 3.08x baseline |

## Reproduction

```bash
python benchmarks/run.py
```


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
| Math Loop (80M iter) | **Opus** (3.66ms) | **TCC** (12.7ms) | **TCC** (2560B) |
| State Accumulation (50M iter) | **Opus** (3.94ms) | **Opus** (7.9ms) | **TCC** (2560B) |
| Branch Parity (40M iter) | **Opus** (3.60ms) | **Opus** (8.0ms) | **TCC** (2560B) |
| General Logic (6M iter) | **MSVC** (14.29ms) | **Opus** (8.2ms) | **TCC** (2560B) |
| HFT Order Book (12M iter) | **GCC** (90.95ms) | **Opus** (8.8ms) | **TCC** (4096B) |

## Detailed Measurements

### Math Loop (80M iter)
*Power-of-2 shifts, adds, and induction operations*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 17.1 | 4,608 B | 3.66 | 3.81 | **Fastest** |
| **GCC** | 3207.1 | 14,848 B | 4.91 | 5.30 | 1.34x baseline |
| **MSVC** | 3552.9 | 107,520 B | 24.85 | 25.03 | 6.78x baseline |
| **TCC** | 12.7 | 2,560 B | 82.89 | 84.25 | 22.63x baseline |

### State Accumulation (50M iter)
*Multiple state updates with scalar induction*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 7.9 | 4,608 B | 3.94 | 4.04 | **Fastest** |
| **GCC** | 85.4 | 14,848 B | 4.68 | 4.98 | 1.19x baseline |
| **MSVC** | 91.3 | 107,520 B | 17.64 | 17.77 | 4.48x baseline |
| **TCC** | 9.1 | 2,560 B | 53.52 | 54.27 | 13.59x baseline |

### Branch Parity (40M iter)
*Alternating parity branch recurrence*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 8.0 | 4,608 B | 3.60 | 3.73 | **Fastest** |
| **GCC** | 335.1 | 14,848 B | 15.79 | 15.94 | 4.38x baseline |
| **MSVC** | 87.3 | 107,520 B | 16.42 | 16.67 | 4.56x baseline |
| **TCC** | 9.3 | 2,560 B | 31.47 | 33.45 | 8.73x baseline |

### General Logic (6M iter)
*Control flow folding, conditionals, and math blend*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 8.2 | 4,608 B | 14.83 | 14.92 | 1.04x baseline |
| **GCC** | 146.5 | 14,848 B | 15.02 | 15.29 | 1.05x baseline |
| **MSVC** | 149.2 | 108,032 B | 14.29 | 14.38 | **Fastest** |
| **TCC** | 9.8 | 2,560 B | 28.22 | 28.71 | 1.97x baseline |

### HFT Order Book (12M iter)
*L2 order book matching, inventory skew, and risk checksums*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 8.8 | 7,168 B | 159.08 | 160.85 | 1.75x baseline |
| **GCC** | 165.3 | 15,360 B | 90.95 | 92.25 | **Fastest** |
| **MSVC** | 110.5 | 108,544 B | 98.99 | 99.33 | 1.09x baseline |
| **TCC** | 9.2 | 4,096 B | 178.46 | 181.76 | 1.96x baseline |

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
| Math Loop (80M iter) | **Opus** (15.99ms) | **TCC** (62.1ms) | **TCC** (2560B) |
| State Accumulation (50M iter) | **Opus** (16.32ms) | **Opus** (32.9ms) | **TCC** (2560B) |
| Branch Parity (40M iter) | **Opus** (15.46ms) | **Opus** (30.3ms) | **TCC** (2560B) |
| General Logic (6M iter) | **Opus** (32.54ms) | **TCC** (35.3ms) | **TCC** (2560B) |
| HFT Order Book (12M iter) | **GCC** (148.27ms) | **TCC** (35.2ms) | **TCC** (4096B) |

## Detailed Measurements

### Math Loop (80M iter)
*Power-of-2 shifts, adds, and induction operations*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 99.5 | 4,608 B | 15.99 | 16.28 | **Fastest** |
| **GCC** | 3725.6 | 14,848 B | 19.47 | 20.19 | 1.22x baseline |
| **TCC** | 62.1 | 2,560 B | 196.68 | 197.36 | 12.30x baseline |

### State Accumulation (50M iter)
*Multiple state updates with scalar induction*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 32.9 | 4,608 B | 16.32 | 16.97 | **Fastest** |
| **GCC** | 254.5 | 14,848 B | 20.34 | 20.64 | 1.25x baseline |
| **TCC** | 36.8 | 2,560 B | 195.99 | 196.88 | 12.01x baseline |

### Branch Parity (40M iter)
*Alternating parity branch recurrence*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 30.3 | 4,608 B | 15.46 | 15.91 | **Fastest** |
| **GCC** | 507.2 | 14,848 B | 39.41 | 39.84 | 2.55x baseline |
| **TCC** | 36.6 | 2,560 B | 105.04 | 105.83 | 6.80x baseline |

### General Logic (6M iter)
*Control flow folding, conditionals, and math blend*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 43.4 | 4,608 B | 32.54 | 33.13 | **Fastest** |
| **GCC** | 380.2 | 15,360 B | 33.42 | 33.81 | 1.03x baseline |
| **TCC** | 35.3 | 2,560 B | 89.20 | 90.13 | 2.74x baseline |

### HFT Order Book (12M iter)
*L2 order book matching, inventory skew, and risk checksums*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 36.5 | 7,168 B | 582.14 | 582.73 | 3.93x baseline |
| **GCC** | 460.1 | 15,872 B | 148.27 | 149.24 | **Fastest** |
| **TCC** | 35.2 | 4,096 B | 472.94 | 475.09 | 3.19x baseline |

## Reproduction

```bash
python benchmarks/run.py
```

