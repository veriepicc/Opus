# Multi-Platform Benchmark Results

Automated multi-host benchmarks recorded on [CI Run](https://github.com/veriepicc/Opus/actions/runs/42390598080) · Source: `4d1effd7c57b`.


---

## Platform: Linux x86_64 (AMD EPYC 9V45 96-Core Processor)

# Benchmark Results

- Date: 2026-11-07
- Host: `Linux x86_64 (AMD EPYC 9V45 96-Core Processor)`
- Toolchains: Opus, GCC, TCC
- Settings: GCC `-O3 -fwrapv`, MSVC `/O2`, TCC default optimization; Opus native x64.

## Summary

| Benchmark | Best Runtime | Fastest Compile | Smallest Binary |
| :--- | :--- | :--- | :--- |
| Math Loop (80M iter) | **Opus** (1343.87ms) | **GCC** (84.3ms) | **TCC** (2560B) |
| State Accumulation (50M iter) | **Opus** (1336.36ms) | **GCC** (24.8ms) | **TCC** (2560B) |
| Branch Parity (40M iter) | **Opus** (1348.64ms) | **GCC** (30.7ms) | **TCC** (2560B) |
| General Logic (6M iter) | **GCC** (1334.02ms) | **GCC** (33.2ms) | **TCC** (2560B) |
| HFT Order Book (12M iter) | **Opus** (1352.83ms) | **GCC** (44.1ms) | **TCC** (4096B) |

## Detailed Measurements

### Math Loop (80M iter)
*Power-of-2 shifts, adds, and induction operations*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 3227.2 | 4,608 B | 1343.87 | 1364.28 | **Fastest** |
| **GCC** | 84.3 | 14,336 B | 1348.94 | 1358.57 | 1.00x baseline |
| **TCC** | 1363.2 | 2,560 B | 1354.98 | 1418.97 | 1.01x baseline |

### State Accumulation (50M iter)
*Multiple state updates with scalar induction*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 1588.4 | 4,608 B | 1336.36 | 1352.59 | **Fastest** |
| **GCC** | 24.8 | 14,336 B | 1349.60 | 1351.99 | 1.01x baseline |
| **TCC** | 1350.8 | 2,560 B | 1343.67 | 1387.04 | 1.01x baseline |

### Branch Parity (40M iter)
*Alternating parity branch recurrence*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 1382.0 | 4,608 B | 1348.64 | 1356.21 | **Fastest** |
| **GCC** | 30.7 | 14,336 B | 1364.80 | 1370.57 | 1.01x baseline |
| **TCC** | 1369.1 | 2,560 B | 1352.54 | 1355.50 | 1.00x baseline |

### General Logic (6M iter)
*Control flow folding, conditionals, and math blend*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 1362.1 | 4,608 B | 1347.56 | 1357.85 | 1.01x baseline |
| **GCC** | 33.2 | 14,336 B | 1334.02 | 1348.34 | **Fastest** |
| **TCC** | 1366.7 | 2,560 B | 1358.00 | 1364.17 | 1.02x baseline |

### HFT Order Book (12M iter)
*L2 order book matching, inventory skew, and risk checksums*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 1352.8 | 7,168 B | 1352.83 | 1377.29 | **Fastest** |
| **GCC** | 44.1 | 14,336 B | 1360.58 | 1408.73 | 1.01x baseline |
| **TCC** | 1372.0 | 4,096 B | 1354.89 | 1400.41 | 1.00x baseline |

## Reproduction

```bash
python benchmarks/run.py
```

## Measurement Errors

- math: toolchain result checksums disagree
- state: toolchain result checksums disagree
- branch: toolchain result checksums disagree
- general: toolchain result checksums disagree
- hft: toolchain result checksums disagree


---

## Platform: Windows AMD64 (INTEL(R) XEON(R) PLATINUM 8573C)

# Benchmark Results

- Date: 2026-11-07
- Host: `Windows AMD64 (INTEL(R) XEON(R) PLATINUM 8573C)`
- Toolchains: Opus, GCC, MSVC, TCC
- Settings: GCC `-O3 -fwrapv`, MSVC `/O2`, TCC default optimization; Opus native x64.

## Summary

| Benchmark | Best Runtime | Fastest Compile | Smallest Binary |
| :--- | :--- | :--- | :--- |
| Math Loop (80M iter) | **Opus** (3.41ms) | **TCC** (11.0ms) | **TCC** (2560B) |
| State Accumulation (50M iter) | **Opus** (3.38ms) | **Opus** (7.5ms) | **TCC** (2560B) |
| Branch Parity (40M iter) | **Opus** (3.30ms) | **Opus** (7.6ms) | **TCC** (2560B) |
| General Logic (6M iter) | **GCC** (13.68ms) | **Opus** (7.6ms) | **TCC** (2560B) |
| HFT Order Book (12M iter) | **GCC** (111.00ms) | **Opus** (7.9ms) | **TCC** (4096B) |

## Detailed Measurements

### Math Loop (80M iter)
*Power-of-2 shifts, adds, and induction operations*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 15.8 | 4,608 B | 3.41 | 3.50 | **Fastest** |
| **GCC** | 3860.8 | 14,848 B | 4.35 | 4.53 | 1.28x baseline |
| **MSVC** | 5019.8 | 108,032 B | 18.04 | 18.53 | 5.30x baseline |
| **TCC** | 11.0 | 2,560 B | 106.76 | 107.81 | 31.35x baseline |

### State Accumulation (50M iter)
*Multiple state updates with scalar induction*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 7.5 | 4,608 B | 3.38 | 3.46 | **Fastest** |
| **GCC** | 85.5 | 14,848 B | 4.32 | 4.51 | 1.28x baseline |
| **MSVC** | 89.5 | 108,032 B | 20.41 | 20.73 | 6.04x baseline |
| **TCC** | 8.1 | 2,560 B | 69.05 | 71.12 | 20.44x baseline |

### Branch Parity (40M iter)
*Alternating parity branch recurrence*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 7.6 | 4,608 B | 3.30 | 3.39 | **Fastest** |
| **GCC** | 310.6 | 14,848 B | 17.32 | 17.76 | 5.24x baseline |
| **MSVC** | 118.1 | 108,032 B | 29.31 | 31.65 | 8.87x baseline |
| **TCC** | 8.1 | 2,560 B | 47.54 | 55.62 | 14.39x baseline |

### General Logic (6M iter)
*Control flow folding, conditionals, and math blend*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 7.6 | 4,608 B | 17.94 | 19.26 | 1.31x baseline |
| **GCC** | 170.9 | 15,360 B | 13.68 | 14.06 | **Fastest** |
| **MSVC** | 96.9 | 108,032 B | 38.94 | 39.70 | 2.85x baseline |
| **TCC** | 8.4 | 2,560 B | 38.33 | 38.70 | 2.80x baseline |

### HFT Order Book (12M iter)
*L2 order book matching, inventory skew, and risk checksums*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 7.9 | 7,168 B | 229.03 | 231.53 | 2.06x baseline |
| **GCC** | 215.2 | 15,872 B | 111.00 | 112.74 | **Fastest** |
| **MSVC** | 107.4 | 108,544 B | 115.69 | 116.22 | 1.04x baseline |
| **TCC** | 9.0 | 4,096 B | 258.14 | 258.79 | 2.33x baseline |

## Reproduction

```bash
python benchmarks/run.py
```


---

## Platform: Windows AMD64 (AMD EPYC 9V74 80-Core Processor)

# Benchmark Results

- Date: 2026-11-07
- Host: `Windows AMD64 (AMD EPYC 9V74 80-Core Processor)`
- Toolchains: Opus, GCC, MSVC, TCC
- Settings: GCC `-O3 -fwrapv`, MSVC `/O2`, TCC default optimization; Opus native x64.

## Summary

| Benchmark | Best Runtime | Fastest Compile | Smallest Binary |
| :--- | :--- | :--- | :--- |
| Math Loop (80M iter) | **Opus** (3.59ms) | **TCC** (12.6ms) | **TCC** (2560B) |
| State Accumulation (50M iter) | **Opus** (3.51ms) | **Opus** (7.8ms) | **TCC** (2560B) |
| Branch Parity (40M iter) | **Opus** (3.48ms) | **Opus** (7.5ms) | **TCC** (2560B) |
| General Logic (6M iter) | **GCC** (15.99ms) | **Opus** (8.9ms) | **TCC** (2560B) |
| HFT Order Book (12M iter) | **GCC** (112.46ms) | **Opus** (9.0ms) | **TCC** (4096B) |

## Detailed Measurements

### Math Loop (80M iter)
*Power-of-2 shifts, adds, and induction operations*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 15.8 | 4,608 B | 3.59 | 3.85 | **Fastest** |
| **GCC** | 3772.7 | 14,848 B | 4.62 | 4.66 | 1.29x baseline |
| **MSVC** | 3456.7 | 107,520 B | 27.40 | 27.48 | 7.64x baseline |
| **TCC** | 12.6 | 2,560 B | 114.73 | 116.16 | 31.98x baseline |

### State Accumulation (50M iter)
*Multiple state updates with scalar induction*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 7.8 | 4,608 B | 3.51 | 3.73 | **Fastest** |
| **GCC** | 85.2 | 14,848 B | 4.58 | 4.67 | 1.31x baseline |
| **MSVC** | 88.8 | 107,520 B | 19.01 | 19.09 | 5.42x baseline |
| **TCC** | 8.8 | 2,560 B | 73.67 | 75.89 | 20.99x baseline |

### Branch Parity (40M iter)
*Alternating parity branch recurrence*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 7.5 | 4,608 B | 3.48 | 3.52 | **Fastest** |
| **GCC** | 400.7 | 14,848 B | 18.11 | 18.70 | 5.20x baseline |
| **MSVC** | 86.0 | 107,520 B | 26.80 | 28.89 | 7.69x baseline |
| **TCC** | 8.2 | 2,560 B | 40.82 | 44.35 | 11.72x baseline |

### General Logic (6M iter)
*Control flow folding, conditionals, and math blend*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 8.9 | 4,608 B | 17.41 | 17.69 | 1.09x baseline |
| **GCC** | 150.9 | 14,848 B | 15.99 | 16.37 | **Fastest** |
| **MSVC** | 102.0 | 108,032 B | 16.85 | 16.93 | 1.05x baseline |
| **TCC** | 9.2 | 2,560 B | 38.93 | 39.09 | 2.44x baseline |

### HFT Order Book (12M iter)
*L2 order book matching, inventory skew, and risk checksums*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 9.0 | 7,168 B | 219.50 | 220.19 | 1.95x baseline |
| **GCC** | 172.2 | 15,360 B | 112.46 | 113.72 | **Fastest** |
| **MSVC** | 111.6 | 108,544 B | 120.41 | 120.50 | 1.07x baseline |
| **TCC** | 9.1 | 4,096 B | 258.79 | 261.30 | 2.30x baseline |

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
| Math Loop (80M iter) | **Opus** (15.95ms) | **TCC** (62.0ms) | **TCC** (2560B) |
| State Accumulation (50M iter) | **Opus** (15.38ms) | **Opus** (30.4ms) | **TCC** (2560B) |
| Branch Parity (40M iter) | **Opus** (15.35ms) | **Opus** (28.6ms) | **TCC** (2560B) |
| General Logic (6M iter) | **Opus** (31.32ms) | **TCC** (34.1ms) | **TCC** (2560B) |
| HFT Order Book (12M iter) | **GCC** (148.23ms) | **TCC** (34.3ms) | **TCC** (4096B) |

## Detailed Measurements

### Math Loop (80M iter)
*Power-of-2 shifts, adds, and induction operations*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 101.6 | 4,608 B | 15.95 | 16.29 | **Fastest** |
| **GCC** | 5024.6 | 14,848 B | 17.85 | 18.63 | 1.12x baseline |
| **TCC** | 62.0 | 2,560 B | 195.16 | 195.50 | 12.24x baseline |

### State Accumulation (50M iter)
*Multiple state updates with scalar induction*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 30.4 | 4,608 B | 15.38 | 15.65 | **Fastest** |
| **GCC** | 239.4 | 14,848 B | 18.27 | 18.60 | 1.19x baseline |
| **TCC** | 32.9 | 2,560 B | 195.64 | 196.12 | 12.72x baseline |

### Branch Parity (40M iter)
*Alternating parity branch recurrence*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 28.6 | 4,608 B | 15.35 | 15.73 | **Fastest** |
| **GCC** | 503.2 | 14,848 B | 37.19 | 37.67 | 2.42x baseline |
| **TCC** | 31.2 | 2,560 B | 103.41 | 103.62 | 6.74x baseline |

### General Logic (6M iter)
*Control flow folding, conditionals, and math blend*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 38.1 | 4,608 B | 31.32 | 31.71 | **Fastest** |
| **GCC** | 365.1 | 15,360 B | 32.10 | 32.32 | 1.02x baseline |
| **TCC** | 34.1 | 2,560 B | 88.84 | 89.05 | 2.84x baseline |

### HFT Order Book (12M iter)
*L2 order book matching, inventory skew, and risk checksums*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 34.8 | 7,168 B | 579.22 | 580.68 | 3.91x baseline |
| **GCC** | 489.0 | 15,872 B | 148.23 | 148.60 | **Fastest** |
| **TCC** | 34.3 | 4,096 B | 470.07 | 471.00 | 3.17x baseline |

## Reproduction

```bash
python benchmarks/run.py
```

