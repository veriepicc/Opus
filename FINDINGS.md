
## Finding `3cab5458b686` — RUNTIME_HANG
- **Timestamp**: `2026-11-07 23:55:44Z`
- **Exit Code**: `-999` (`0xfffffc19`)
- **Reason**: Executable Fault during --run: Compiler Hang / Infinite Loop (>10s)
- **Target Mode**: `types`
- **Artifact File**: `fuzzer/crashes/crash_runtime_hang_3cab5458b686.sb`

### Minimal Reproducer:
```sable
// Auto-generated High-Validity Sable Module

view Header_33517 {
    magic: int @ 0;
    flags: int @ 8;
}

function int helper_16814(int a_46634, int b_3812) {
    a_46634 = 255;
    b_3812 = 1000;
    switch a_46634 {
        case 0: return 0;
        case 1: return 1;
        case 2: return 2;
        case 3: return 3;
        else: return 0;
    }
    a_46634 = a_46634;
    if ((b_3812 / a_46634) < (a_46634 | (5 ^ 2))) {
        print_int(b_3812);
    } else {
        for i_73239 in 0..5 {
            if false {
                print_int(i_73239);
            }
        }
    }
    b_3812 = ((b_3812 % b_3812) % b_3812);
    return -5;
}

function int main() {
    let res = helper_16814(10, 20);
    print_int(res);
    let x_51347 = 5;
    let x_55730 = ((x_51347 + x_51347) - x_51347);
    for i_59700 in 0..5 {
        i_59700 = (0 - -1);
    }
    return 0;
}
```

### Compiler Output:
```text
0
```
---

## Finding `451a3a6dbffc` — RUNTIME_HANG
- **Timestamp**: `2026-11-07 23:55:51Z`
- **Exit Code**: `-999` (`0xfffffc19`)
- **Reason**: Executable Fault during --run: Compiler Hang / Infinite Loop (>10s)
- **Target Mode**: `types`
- **Artifact File**: `fuzzer/crashes/crash_runtime_hang_451a3a6dbffc.sb`

### Minimal Reproducer:
```sable
// Auto-generated High-Validity Sable Module

struct Point_73949 {
    x: int;
    y: int;
}

view Header_29126 {
    magic: int @ 0;
    flags: int @ 8;
}

function int helper_24685(int a_14107, int b_80173) {
    let x_1987 = a_14107;
    for i_71612 in 1..10 {
        i_71612 = ((-42 ^ 255) ^ b_80173);
    }
    if true {
        let x_66340 = (b_80173 & (i_71612 - 5));
    }
    return x_1987;
}

function int main() {
    let res = helper_24685(10, 20);
    print_int(res);
    for i_32920 in 0..5 {
        let x_66552 = i_32920;
    }
    let x_6968 = (5 * 100);
    return 0;
}
```

### Compiler Output:
```text

```
---

## Finding `0367fb0fe311` — RUNTIME_HANG
- **Timestamp**: `2026-11-07 23:57:53Z`
- **Exit Code**: `-999` (`0xfffffc19`)
- **Reason**: Executable Fault during --run: Compiler Hang / Infinite Loop (>10s)
- **Target Mode**: `types`
- **Artifact File**: `fuzzer/crashes/crash_runtime_hang_0367fb0fe311.sb`

### Minimal Reproducer:
```sable
// Auto-generated High-Validity Sable Module

struct Point_27633 {
    x: int;
    y: int;
}

function int helper_42520(int a_15869, int b_3818) {
    let x_33271 = (5 % a_15869);
    switch x_33271 {
        case 0: return 0;
        case 1: return 1;
        case 2: return 2;
        case 3: return 3;
        else: return 0;
    }
    let x_12680 = ((x_33271 - a_15869) & (a_15869 / b_3818));
    switch a_15869 {
        case 0: return 0;
        case 1: return 1;
        else: return 0;
    }
    return (2 ^ (b_3818 - a_15869));
}

function int main() {
    let res = helper_42520(10, 20);
    print_int(res);
    print_int((10 * 255));
    print_int(1);
    let x_16218 = (1 * (255 ^ 3));
    for i_46607 in 1..5 {
        i_46607 = (2 - x_16218);
    }
    return 0;
}
```

### Compiler Output:
```text
0
2550
1
```
---

## Finding `d97d55f058d0` — RUNTIME_HANG
- **Timestamp**: `2026-11-07 23:58:08Z`
- **Exit Code**: `-999` (`0xfffffc19`)
- **Reason**: Executable Fault during --run: Compiler Hang / Infinite Loop (>10s)
- **Target Mode**: `types`
- **Artifact File**: `fuzzer/crashes/crash_runtime_hang_d97d55f058d0.sb`

### Minimal Reproducer:
```sable
// Auto-generated High-Validity Sable Module

function int helper_31470(int a_49605, int b_45432) {
    if (((a_49605 + b_45432) ^ (-10 % 2)) <= ((a_49605 | 2) * a_49605)) {
        print_int(-5);
    }
    switch b_45432 {
        case 0: return 0;
        case 1: return 1;
        else: return 0;
    }
    print_int((a_49605 & -42));
    print_int(a_49605);
    let x_93631 = b_45432;
    return ((x_93631 + b_45432) / 5);
}

function int main() {
    let res = helper_31470(10, 20);
    print_int(res);
    print_int((-10 - -5));
    for i_65760 in 1..5 {
        i_65760 = -1;
    }
    return 0;
}
```

### Compiler Output:
```text
-5
0
-5
```
---

## Finding `fa2eca37e634` — RUNTIME_HANG
- **Timestamp**: `2026-11-07 23:58:41Z`
- **Exit Code**: `-999` (`0xfffffc19`)
- **Reason**: Executable Fault during --run: Compiler Hang / Infinite Loop (>10s)
- **Target Mode**: `types`
- **Artifact File**: `fuzzer/crashes/crash_runtime_hang_fa2eca37e634.sb`

### Minimal Reproducer:
```sable
// Auto-generated High-Validity Sable Module

view Header_51798 {
    magic: int @ 0;
    flags: int @ 8;
}

function int helper_40621(int a_55522, int b_58670) {
    let x_27772 = (b_58670 / -42);
    switch b_58670 {
        case 0: return 0;
        case 1: return 1;
        case 2: return 2;
        case 3: return 3;
        else: return 0;
    }
    let x_16931 = 5;
    print_int((5 & x_16931));
    return (0 & (x_27772 & x_27772));
}

function int main() {
    let res = helper_40621(10, 20);
    print_int(res);
    let x_32224 = ((255 | 1000) * (-1 & 1000));
    print_int((x_32224 & x_32224));
    for i_61401 in 0..5 {
        i_61401 = 0;
    }
    return 0;
}
```

### Compiler Output:
```text
0
1023000
```
---

## Finding `7a76e0cabcec` — RUNTIME_HANG
- **Timestamp**: `2026-11-07 23:59:00Z`
- **Exit Code**: `-999` (`0xfffffc19`)
- **Reason**: Executable Fault during --run: Compiler Hang / Infinite Loop (>10s)
- **Target Mode**: `types`
- **Artifact File**: `fuzzer/crashes/crash_runtime_hang_7a76e0cabcec.sb`

### Minimal Reproducer:
```sable
// Auto-generated High-Validity Sable Module

struct Point_92317 {
    x: int;
    y: int;
}

view Header_9191 {
    magic: int @ 0;
    flags: int @ 8;
}

function int helper_89954(int a_89232, int b_45938) {
    print_int((-42 | b_45938));
    switch b_45938 {
        case 0: return 0;
        case 1: return 1;
        else: return 0;
    }
    b_45938 = a_89232;
    let x_31254 = (2 & b_45938);
    return ((x_31254 - 10) ^ (x_31254 ^ x_31254));
}

function int main() {
    let res = helper_89954(10, 20);
    print_int(res);
    for i_99683 in 0..10 {
        i_99683 = (i_99683 ^ (i_99683 * i_99683));
    }
    if false {
        let x_50525 = i_99683;
    } else {
        let x_2616 = (i_99683 ^ (100 * 1));
    }
    for i_99430 in 0..5 {
        let x_55878 = (-1 / -10);
    }
    return 0;
}
```

### Compiler Output:
```text
-42
0
```
---

## Finding `d23ba949d308` — RUNTIME_HANG
- **Timestamp**: `2026-11-07 23:59:38Z`
- **Exit Code**: `-999` (`0xfffffc19`)
- **Reason**: Executable Fault during --run: Compiler Hang / Infinite Loop (>10s)
- **Target Mode**: `types`
- **Artifact File**: `fuzzer/crashes/crash_runtime_hang_d23ba949d308.sb`

### Minimal Reproducer:
```sable
// Auto-generated High-Validity Sable Module

view Header_80738 {
    magic: int @ 0;
    flags: int @ 8;
}

function int helper_51364(int a_37004, int b_90534) {
    let x_37576 = ((5 | b_90534) - -1);
    print_int(a_37004);
    a_37004 = (a_37004 / -42);
    return ((-5 - a_37004) + 10);
}

function int main() {
    let res = helper_51364(10, 20);
    print_int(res);
    print_int((-42 ^ 5));
    if false {
        print_int(1000);
    } else {
        for i_60035 in 1..5 {
            i_60035 = ((i_60035 ^ i_60035) - i_60035);
        }
    }
    return 0;
}
```

### Compiler Output:
```text
10
5
-45
```
---

## Finding `f84f48cc0c67` — RUNTIME_HANG
- **Timestamp**: `2026-11-07 23:59:44Z`
- **Exit Code**: `-999` (`0xfffffc19`)
- **Reason**: Executable Fault during --run: Compiler Hang / Infinite Loop (>10s)
- **Target Mode**: `types`
- **Artifact File**: `fuzzer/crashes/crash_runtime_hang_f84f48cc0c67.sb`

### Minimal Reproducer:
```sable
// Auto-generated High-Validity Sable Module

struct Point_25831 {
    x: int;
    y: int;
}

view Header_55502 {
    magic: int @ 0;
    flags: int @ 8;
}

function int helper_34535(int a_68401, int b_21272) {
    let x_68221 = ((b_21272 - a_68401) * -42);
    print_int((-42 | 5));
    let x_67843 = ((b_21272 | 100) / (a_68401 / a_68401));
    let x_73873 = ((x_67843 + x_67843) % (x_67843 & a_68401));
    for i_46558 in 1..10 {
        i_46558 = (-42 - (10 | x_67843));
    }
    return x_68221;
}

function int main() {
    let res = helper_34535(10, 20);
    print_int(res);
    let x_33746 = 1000;
    let x_98570 = x_33746;
    let x_17794 = 2;
    let x_95507 = ((x_98570 & x_17794) ^ (-5 | x_98570));
    return 0;
}
```

### Compiler Output:
```text
-41
```
---
