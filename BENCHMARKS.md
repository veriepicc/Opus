# Multi-Platform Benchmark Results

Automated multi-host benchmarks recorded on [CI Run](https://github.com/veriepicc/Opus/actions/runs/42391108095) · Source: `0c2cdb13f741`.


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
| Math Loop (80M iter) | **GCC** (1344.52ms) | **GCC** (104.2ms) | **TCC** (2560B) |
| State Accumulation (50M iter) | **Opus** (1347.97ms) | **GCC** (24.3ms) | **TCC** (2560B) |
| Branch Parity (40M iter) | **GCC** (1356.13ms) | **GCC** (31.0ms) | **TCC** (2560B) |
| General Logic (6M iter) | **Opus** (1337.50ms) | **GCC** (29.7ms) | **TCC** (2560B) |
| HFT Order Book (12M iter) | **TCC** (1328.66ms) | **GCC** (44.4ms) | **TCC** (4096B) |

## Detailed Measurements

### Math Loop (80M iter)
*Power-of-2 shifts, adds, and induction operations*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 3200.3 | 4,608 B | 1347.05 | 1351.49 | 1.00x baseline |
| **GCC** | 104.2 | 14,336 B | 1344.52 | 1352.27 | **Fastest** |
| **TCC** | 1361.9 | 2,560 B | 1352.41 | 1422.98 | 1.01x baseline |

### State Accumulation (50M iter)
*Multiple state updates with scalar induction*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 1488.7 | 4,608 B | 1347.97 | 1369.60 | **Fastest** |
| **GCC** | 24.3 | 14,336 B | 1355.47 | 1363.03 | 1.01x baseline |
| **TCC** | 1361.9 | 2,560 B | 1369.72 | 1380.54 | 1.02x baseline |

### Branch Parity (40M iter)
*Alternating parity branch recurrence*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 1402.3 | 4,608 B | 1358.60 | 1366.65 | 1.00x baseline |
| **GCC** | 31.0 | 14,336 B | 1356.13 | 1364.52 | **Fastest** |
| **TCC** | 1355.4 | 2,560 B | 1359.23 | 1369.11 | 1.00x baseline |

### General Logic (6M iter)
*Control flow folding, conditionals, and math blend*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 1355.6 | 4,608 B | 1337.50 | 1347.47 | **Fastest** |
| **GCC** | 29.7 | 14,336 B | 1338.05 | 1349.85 | 1.00x baseline |
| **TCC** | 1335.9 | 2,560 B | 1343.26 | 1363.48 | 1.00x baseline |

### HFT Order Book (12M iter)
*L2 order book matching, inventory skew, and risk checksums*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 1353.3 | 7,168 B | 1359.55 | 1370.57 | 1.02x baseline |
| **GCC** | 44.4 | 14,336 B | 1345.37 | 1350.48 | 1.01x baseline |
| **TCC** | 1356.2 | 4,096 B | 1328.66 | 1353.50 | **Fastest** |

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

## Platform: Windows AMD64 (AMD EPYC 7763 64-Core Processor)

# Benchmark Results

- Date: 2026-11-07
- Host: `Windows AMD64 (AMD EPYC 7763 64-Core Processor)`
- Toolchains: Opus, GCC, MSVC, TCC
- Settings: GCC `-O3 -fwrapv`, MSVC `/O2`, TCC default optimization; Opus native x64.

## Summary

| Benchmark | Best Runtime | Fastest Compile | Smallest Binary |
| :--- | :--- | :--- | :--- |
| Math Loop (80M iter) | **Opus** (3.03ms) | **TCC** (10.5ms) | **TCC** (2560B) |
| State Accumulation (50M iter) | **Opus** (3.01ms) | **Opus** (6.8ms) | **TCC** (2560B) |
| Branch Parity (40M iter) | **Opus** (3.01ms) | **Opus** (7.1ms) | **TCC** (2560B) |
| General Logic (6M iter) | **GCC** (16.29ms) | **Opus** (7.7ms) | **TCC** (2560B) |
| HFT Order Book (12M iter) | **MSVC** (127.57ms) | **TCC** (8.1ms) | **TCC** (4096B) |

## Detailed Measurements

### Math Loop (80M iter)
*Power-of-2 shifts, adds, and induction operations*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 16.7 | 4,608 B | 3.03 | 3.14 | **Fastest** |
| **GCC** | 3198.7 | 14,848 B | 4.00 | 4.15 | 1.32x baseline |
| **MSVC** | 4378.3 | 108,032 B | 29.02 | 29.20 | 9.58x baseline |
| **TCC** | 10.5 | 2,560 B | 128.82 | 131.16 | 42.52x baseline |

### State Accumulation (50M iter)
*Multiple state updates with scalar induction*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 6.8 | 4,608 B | 3.01 | 3.05 | **Fastest** |
| **GCC** | 83.0 | 14,848 B | 3.98 | 4.09 | 1.32x baseline |
| **MSVC** | 86.4 | 108,032 B | 21.74 | 21.84 | 7.23x baseline |
| **TCC** | 7.6 | 2,560 B | 82.10 | 82.44 | 27.31x baseline |

### Branch Parity (40M iter)
*Alternating parity branch recurrence*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 7.1 | 4,608 B | 3.01 | 3.09 | **Fastest** |
| **GCC** | 260.6 | 14,848 B | 18.29 | 18.37 | 6.08x baseline |
| **MSVC** | 89.4 | 108,032 B | 22.76 | 23.19 | 7.57x baseline |
| **TCC** | 7.9 | 2,560 B | 49.27 | 49.41 | 16.39x baseline |

### General Logic (6M iter)
*Control flow folding, conditionals, and math blend*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 7.7 | 4,608 B | 20.20 | 20.51 | 1.24x baseline |
| **GCC** | 153.9 | 15,360 B | 16.29 | 16.56 | **Fastest** |
| **MSVC** | 100.2 | 108,032 B | 38.35 | 38.57 | 2.35x baseline |
| **TCC** | 8.2 | 2,560 B | 46.32 | 46.54 | 2.84x baseline |

### HFT Order Book (12M iter)
*L2 order book matching, inventory skew, and risk checksums*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 8.1 | 7,168 B | 420.28 | 421.70 | 3.29x baseline |
| **GCC** | 200.9 | 15,872 B | 128.81 | 129.72 | 1.01x baseline |
| **MSVC** | 105.2 | 108,544 B | 127.57 | 127.92 | **Fastest** |
| **TCC** | 8.1 | 4,096 B | 393.80 | 394.97 | 3.09x baseline |

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
| Math Loop (80M iter) | **Opus** (4.39ms) | **TCC** (14.1ms) | **TCC** (2560B) |
| State Accumulation (50M iter) | **Opus** (4.32ms) | **Opus** (8.6ms) | **TCC** (2560B) |
| Branch Parity (40M iter) | **Opus** (4.25ms) | **Opus** (8.5ms) | **TCC** (2560B) |
| General Logic (6M iter) | **GCC** (17.84ms) | **Opus** (9.1ms) | **TCC** (2560B) |
| HFT Order Book (12M iter) | **GCC** (130.96ms) | **Opus** (9.4ms) | **TCC** (4096B) |

## Detailed Measurements

### Math Loop (80M iter)
*Power-of-2 shifts, adds, and induction operations*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 20.3 | 4,608 B | 4.39 | 4.46 | **Fastest** |
| **GCC** | 3077.0 | 14,848 B | 5.62 | 5.67 | 1.28x baseline |
| **MSVC** | 3500.4 | 107,520 B | 30.34 | 30.46 | 6.91x baseline |
| **TCC** | 14.1 | 2,560 B | 130.41 | 133.56 | 29.72x baseline |

### State Accumulation (50M iter)
*Multiple state updates with scalar induction*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 8.6 | 4,608 B | 4.32 | 4.42 | **Fastest** |
| **GCC** | 100.7 | 14,848 B | 5.53 | 5.63 | 1.28x baseline |
| **MSVC** | 101.3 | 107,520 B | 21.09 | 21.32 | 4.88x baseline |
| **TCC** | 9.8 | 2,560 B | 83.57 | 85.76 | 19.33x baseline |

### Branch Parity (40M iter)
*Alternating parity branch recurrence*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 8.5 | 4,608 B | 4.25 | 4.44 | **Fastest** |
| **GCC** | 385.2 | 14,848 B | 20.20 | 20.38 | 4.75x baseline |
| **MSVC** | 100.5 | 107,520 B | 30.41 | 30.52 | 7.15x baseline |
| **TCC** | 9.9 | 2,560 B | 50.38 | 50.74 | 11.84x baseline |

### General Logic (6M iter)
*Control flow folding, conditionals, and math blend*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 9.1 | 4,608 B | 21.32 | 21.49 | 1.20x baseline |
| **GCC** | 171.7 | 14,848 B | 17.84 | 17.88 | **Fastest** |
| **MSVC** | 112.7 | 108,032 B | 18.54 | 18.63 | 1.04x baseline |
| **TCC** | 9.9 | 2,560 B | 47.46 | 47.54 | 2.66x baseline |

### HFT Order Book (12M iter)
*L2 order book matching, inventory skew, and risk checksums*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 9.4 | 7,168 B | 427.40 | 430.78 | 3.26x baseline |
| **GCC** | 199.1 | 15,360 B | 130.96 | 133.92 | **Fastest** |
| **MSVC** | 124.7 | 108,544 B | 140.14 | 141.61 | 1.07x baseline |
| **TCC** | 10.6 | 4,096 B | 401.21 | 406.16 | 3.06x baseline |

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
| Math Loop (80M iter) | **Opus** (14.98ms) | **TCC** (55.9ms) | **TCC** (2560B) |
| State Accumulation (50M iter) | **Opus** (15.79ms) | **Opus** (31.1ms) | **TCC** (2560B) |
| Branch Parity (40M iter) | **Opus** (15.36ms) | **Opus** (29.4ms) | **TCC** (2560B) |
| General Logic (6M iter) | **GCC** (31.47ms) | **TCC** (31.7ms) | **TCC** (2560B) |
| HFT Order Book (12M iter) | **GCC** (147.80ms) | **TCC** (33.2ms) | **TCC** (4096B) |

## Detailed Measurements

### Math Loop (80M iter)
*Power-of-2 shifts, adds, and induction operations*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 115.2 | 4,608 B | 14.98 | 15.44 | **Fastest** |
| **GCC** | 4785.4 | 14,848 B | 18.29 | 18.56 | 1.22x baseline |
| **TCC** | 55.9 | 2,560 B | 195.02 | 196.27 | 13.02x baseline |

### State Accumulation (50M iter)
*Multiple state updates with scalar induction*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 31.1 | 4,608 B | 15.79 | 15.92 | **Fastest** |
| **GCC** | 243.7 | 14,848 B | 19.33 | 19.63 | 1.22x baseline |
| **TCC** | 33.8 | 2,560 B | 196.29 | 199.26 | 12.43x baseline |

### Branch Parity (40M iter)
*Alternating parity branch recurrence*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 29.4 | 4,608 B | 15.36 | 15.64 | **Fastest** |
| **GCC** | 494.3 | 14,848 B | 38.08 | 38.66 | 2.48x baseline |
| **TCC** | 33.9 | 2,560 B | 103.79 | 104.69 | 6.76x baseline |

### General Logic (6M iter)
*Control flow folding, conditionals, and math blend*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 39.8 | 4,608 B | 31.52 | 31.99 | 1.00x baseline |
| **GCC** | 362.3 | 15,360 B | 31.47 | 32.04 | **Fastest** |
| **TCC** | 31.7 | 2,560 B | 88.03 | 88.61 | 2.80x baseline |

### HFT Order Book (12M iter)
*L2 order book matching, inventory skew, and risk checksums*

| Toolchain | Compile (ms) | Binary Size | Runtime Min (ms) | Runtime Avg (ms) | Performance |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Opus** | 33.3 | 7,168 B | 578.90 | 579.60 | 3.92x baseline |
| **GCC** | 439.6 | 15,872 B | 147.80 | 148.12 | **Fastest** |
| **TCC** | 33.2 | 4,096 B | 469.02 | 470.66 | 3.17x baseline |

## Reproduction

```bash
python benchmarks/run.py
```

