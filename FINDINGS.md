
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

## Finding `729c301622c8` — RUNTIME_HANG
- **Timestamp**: `2026-11-07 23:59:59Z`
- **Exit Code**: `-999` (`0xfffffc19`)
- **Reason**: Executable Fault during --run: Compiler Hang / Infinite Loop (>10s)
- **Target Mode**: `types`
- **Artifact File**: `fuzzer/crashes/crash_runtime_hang_729c301622c8.sb`

### Minimal Reproducer:
```sable
// Auto-generated High-Validity Sable Module

struct Point_35483 {
    x: int;
    y: int;
}

function int helper_23535(int a_9050, int b_96290) {
    switch b_96290 {
        case 0: return 0;
        case 1: return 1;
        case 2: return 2;
        else: return 0;
    }
    switch a_9050 {
        case 0: return 0;
        case 1: return 1;
        else: return 0;
    }
    b_96290 = 1;
    let x_47012 = a_9050;
    let x_15602 = 0;
    print_int((x_15602 % x_47012));
    return ((a_9050 & x_47012) & (x_15602 & b_96290));
}

function int main() {
    let res = helper_23535(10, 20);
    print_int(res);
    for i_20482 in 1..10 {
        i_20482 = 3;
    }
    let x_50762 = 42;
    for i_73388 in 1..5 {
        switch x_50762 {
            case 0: return 0;
            case 1: return 1;
            case 2: return 2;
            case 3: return 3;
            else: return 0;
        }
    }
    let x_11742 = ((i_73388 - 5) * (i_20482 + 100));
    return 0;
}
```

### Compiler Output:
```text
0
```
---

## Finding `7a70b3dff517` — RUNTIME_HANG
- **Timestamp**: `2026-11-08 00:00:56Z`
- **Exit Code**: `-999` (`0xfffffc19`)
- **Reason**: Executable Fault during --run: Compiler Hang / Infinite Loop (>10s)
- **Target Mode**: `types`
- **Artifact File**: `fuzzer/crashes/crash_runtime_hang_7a70b3dff517.sb`

### Minimal Reproducer:
```sable
// Auto-generated High-Validity Sable Module

function int helper_3978(int a_74129, int b_21601) {
    for i_37428 in 0..10 {
        for i_31482 in 1..10 {
            i_31482 = i_37428;
        }
    }
    let x_50480 = 42;
    switch x_50480 {
        case 0: return 0;
        case 1: return 1;
        else: return 0;
    }
    let x_27054 = -1;
    return x_50480;
}

function int main() {
    let res = helper_3978(10, 20);
    print_int(res);
    switch 0 {
        case 0: return 0;
        case 1: return 1;
        case 2: return 2;
        case 3: return 3;
        else: return 0;
    }
    let x_55215 = ((5 - 1000) % (100 + 2));
    let x_67424 = (3 | (2 - x_55215));
    print_int((x_55215 - -5));
    let x_55476 = 5;
    return 0;
}
```

### Compiler Output:
```text

```
---

## Finding `40c647352920` — RUNTIME_HANG
- **Timestamp**: `2026-11-08 00:01:04Z`
- **Exit Code**: `-999` (`0xfffffc19`)
- **Reason**: Executable Fault during --run: Compiler Hang / Infinite Loop (>10s)
- **Target Mode**: `types`
- **Artifact File**: `fuzzer/crashes/crash_runtime_hang_40c647352920.sb`

### Minimal Reproducer:
```sable
// Auto-generated High-Validity Sable Module

struct Point_93942 {
    x: int;
    y: int;
}

function int helper_94714(int a_53999, int b_52896) {
    let x_44594 = a_53999;
    print_int((0 + x_44594));
    let x_89601 = ((-42 + b_52896) / 2);
    return ((3 | b_52896) | (b_52896 + x_44594));
}

function int main() {
    let res = helper_94714(10, 20);
    print_int(res);
    for i_26644 in 1..10 {
        i_26644 = ((i_26644 % i_26644) + (i_26644 ^ i_26644));
    }
    for i_464 in 1..10 {
        print_int(2);
    }
    return 0;
}
```

### Compiler Output:
```text
10
31
```
---

## Finding `f79b96508e82` — RUNTIME_HANG
- **Timestamp**: `2026-11-08 00:01:24Z`
- **Exit Code**: `-999` (`0xfffffc19`)
- **Reason**: Executable Fault during --run: Compiler Hang / Infinite Loop (>10s)
- **Target Mode**: `types`
- **Artifact File**: `fuzzer/crashes/crash_runtime_hang_f79b96508e82.sb`

### Minimal Reproducer:
```sable
// Auto-generated High-Validity Sable Module

function int helper_52283(int a_67239, int b_61331) {
    let x_52905 = (a_67239 / b_61331);
    switch b_61331 {
        case 0: return 0;
        case 1: return 1;
        case 2: return 2;
        case 3: return 3;
        else: return 0;
    }
    b_61331 = -10;
    switch a_67239 {
        case 0: return 0;
        case 1: return 1;
        case 2: return 2;
        case 3: return 3;
        else: return 0;
    }
    let x_25897 = ((2 | 10) / (-5 * b_61331));
    let x_43954 = 1000;
    let x_31821 = ((x_43954 - 42) * (1 / a_67239));
    return ((x_31821 / 42) & (x_31821 + x_25897));
}

function int main() {
    let res = helper_52283(10, 20);
    print_int(res);
    if true {
        print_int((5 | 255));
    }
    let x_89612 = -1;
    for i_59371 in 1..5 {
        i_59371 = x_89612;
    }
    let x_66474 = -42;
    return 0;
}
```

### Compiler Output:
```text
0
255
```
---

## Finding `9a58016b5442` — RUNTIME_HANG
- **Timestamp**: `2026-11-08 00:02:18Z`
- **Exit Code**: `-999` (`0xfffffc19`)
- **Reason**: Executable Fault during --run: Compiler Hang / Infinite Loop (>10s)
- **Target Mode**: `types`
- **Artifact File**: `fuzzer/crashes/crash_runtime_hang_9a58016b5442.sb`

### Minimal Reproducer:
```sable
// Auto-generated High-Validity Sable Module

function int helper_22642(int a_45820, int b_85132) {
    let x_45208 = b_85132;
    switch x_45208 {
        case 0: return 0;
        case 1: return 1;
        else: return 0;
    }
    print_int(x_45208);
    let x_54172 = (b_85132 * (b_85132 / -42));
    b_85132 = (3 & (a_45820 * a_45820));
    switch x_54172 {
        case 0: return 0;
        case 1: return 1;
        case 2: return 2;
        case 3: return 3;
        else: return 0;
    }
    let x_57278 = ((a_45820 + x_54172) * (1000 + 5));
    return 5;
}

function int main() {
    let res = helper_22642(10, 20);
    print_int(res);
    let x_20512 = ((42 * 1000) * (5 & 42));
    for i_11426 in 1..5 {
        for i_55405 in 0..10 {
            i_11426 = x_20512;
        }
    }
    return 0;
}
```

### Compiler Output:
```text
0
```
---

## Finding `14a3d22b4e8b` — RUNTIME_HANG
- **Timestamp**: `2026-11-08 00:02:30Z`
- **Exit Code**: `-999` (`0xfffffc19`)
- **Reason**: Executable Fault during --run: Compiler Hang / Infinite Loop (>10s)
- **Target Mode**: `types`
- **Artifact File**: `fuzzer/crashes/crash_runtime_hang_14a3d22b4e8b.sb`

### Minimal Reproducer:
```sable
// Auto-generated High-Validity Sable Module

struct Point_4994 {
    x: int;
    y: int;
}

function int helper_24831(int a_99865, int b_26034) {
    if (b_26034 <= ((5 - 2) - b_26034)) {
        print_int((a_99865 ^ a_99865));
    }
    if (5 < (b_26034 & (3 ^ a_99865))) {
        let x_47387 = b_26034;
    }
    for i_22266 in 0..10 {
        i_22266 = (3 + (b_26034 % b_26034));
    }
    return -1;
}

function int main() {
    let res = helper_24831(10, 20);
    print_int(res);
    print_int((2 * 255));
    let x_67106 = (-1 % -1);
    let x_24129 = -42;
    print_int(x_67106);
    return 0;
}
```

### Compiler Output:
```text

```
---

## Finding `8a7c2c08a15a` — RUNTIME_HANG
- **Timestamp**: `2026-11-08 00:02:37Z`
- **Exit Code**: `-999` (`0xfffffc19`)
- **Reason**: Executable Fault during --run: Compiler Hang / Infinite Loop (>10s)
- **Target Mode**: `types`
- **Artifact File**: `fuzzer/crashes/crash_runtime_hang_8a7c2c08a15a.sb`

### Minimal Reproducer:
```sable
// Auto-generated High-Validity Sable Module

view Header_63204 {
    magic: int @ 0;
    flags: int @ 8;
}

function int helper_65525(int a_60063, int b_73210) {
    if (((100 ^ 255) & (a_60063 & -1)) >= ((-5 % 255) & (0 / a_60063))) {
        b_73210 = (2 * (5 | a_60063));
    }
    if false {
        switch b_73210 {
            case 0: return 0;
            case 1: return 1;
            case 2: return 2;
            else: return 0;
        }
    }
    print_int(a_60063);
    if true {
        switch a_60063 {
            case 0: return 0;
            case 1: return 1;
            case 2: return 2;
            case 3: return 3;
            else: return 0;
        }
    }
    return 3;
}

function int main() {
    let res = helper_65525(10, 20);
    print_int(res);
    print_int((255 % 3));
    for i_64004 in 0..5 {
        i_64004 = (-1 / 42);
    }
    return 0;
}
```

### Compiler Output:
```text
10
0
0
```
---

## Finding `13f773216022` — RUNTIME_HANG
- **Timestamp**: `2026-11-08 00:02:54Z`
- **Exit Code**: `-999` (`0xfffffc19`)
- **Reason**: Executable Fault during --run: Compiler Hang / Infinite Loop (>10s)
- **Target Mode**: `types`
- **Artifact File**: `fuzzer/crashes/crash_runtime_hang_13f773216022.sb`

### Minimal Reproducer:
```sable
// Auto-generated High-Validity Sable Module

struct Point_34525 {
    x: int;
    y: int;
}

view Header_9974 {
    magic: int @ 0;
    flags: int @ 8;
}

function int helper_63255(int a_2812, int b_79667) {
    if false {
        let x_36003 = 255;
    }
    for i_78344 in 0..10 {
        for i_88614 in 1..10 {
            let x_99497 = ((a_2812 * a_2812) - (a_2812 * 2));
        }
    }
    for i_20184 in 1..5 {
        for i_43841 in 1..10 {
            i_43841 = ((42 ^ b_79667) ^ -42);
        }
    }
    return a_2812;
}

function int main() {
    let res = helper_63255(10, 20);
    print_int(res);
    if (255 >= (100 & -42)) {
        if ((2 | (-5 ^ 3)) <= ((10 - 42) - (-42 - 3))) {
            print_int((255 + 3));
        }
    } else {
        print_int((3 ^ -5));
    }
    switch 0 {
        case 0: return 0;
        case 1: return 1;
        else: return 0;
    }
    return 0;
}
```

### Compiler Output:
```text

```
---

## Finding `3ebdef1a5798` — RUNTIME_HANG
- **Timestamp**: `2026-11-08 00:03:00Z`
- **Exit Code**: `-999` (`0xfffffc19`)
- **Reason**: Executable Fault during --run: Compiler Hang / Infinite Loop (>10s)
- **Target Mode**: `types`
- **Artifact File**: `fuzzer/crashes/crash_runtime_hang_3ebdef1a5798.sb`

### Minimal Reproducer:
```sable
// Auto-generated High-Validity Sable Module

function int helper_33716(int a_51881, int b_87206) {
    switch b_87206 {
        case 0: return 0;
        case 1: return 1;
        else: return 0;
    }
    switch a_51881 {
        case 0: return 0;
        case 1: return 1;
        case 2: return 2;
        else: return 0;
    }
    for i_51720 in 0..10 {
        if true {
            b_87206 = (i_51720 * i_51720);
        } else {
            let x_2069 = ((3 ^ a_51881) / (42 ^ i_51720));
        }
    }
    let x_33308 = ((-1 * 100) + i_51720);
    return -10;
}

function int main() {
    let res = helper_33716(10, 20);
    print_int(res);
    for i_62365 in 0..5 {
        i_62365 = ((i_62365 + i_62365) ^ (i_62365 + i_62365));
    }
    print_int((0 * 1));
    return 0;
}
```

### Compiler Output:
```text
0
```
---

## Finding `2116032b3249` — RUNTIME_HANG
- **Timestamp**: `2026-11-08 00:03:51Z`
- **Exit Code**: `-999` (`0xfffffc19`)
- **Reason**: Executable Fault during --run: Compiler Hang / Infinite Loop (>10s)
- **Target Mode**: `types`
- **Artifact File**: `fuzzer/crashes/crash_runtime_hang_2116032b3249.sb`

### Minimal Reproducer:
```sable
// Auto-generated High-Validity Sable Module

view Header_24303 {
    magic: int @ 0;
    flags: int @ 8;
}

function int helper_71350(int a_76343, int b_90697) {
    print_int(a_76343);
    b_90697 = 0;
    let x_51975 = ((b_90697 * a_76343) ^ (a_76343 - 10));
    return ((1000 - 100) & (1000 - a_76343));
}

function int main() {
    let res = helper_71350(10, 20);
    print_int(res);
    print_int((10 ^ 10));
    print_int((1 | 100));
    let x_3558 = (-5 ^ (2 & -1));
    let x_6019 = -1;
    for i_35625 in 1..10 {
        i_35625 = x_6019;
    }
    return 0;
}
```

### Compiler Output:
```text
10
900
0
101
```
---

## Finding `b0445ecd2e76` — RUNTIME_HANG
- **Timestamp**: `2026-11-08 00:04:56Z`
- **Exit Code**: `-999` (`0xfffffc19`)
- **Reason**: Executable Fault during --run: Compiler Hang / Infinite Loop (>10s)
- **Target Mode**: `types`
- **Artifact File**: `fuzzer/crashes/crash_runtime_hang_b0445ecd2e76.sb`

### Minimal Reproducer:
```sable
// Auto-generated High-Validity Sable Module

struct Point_30210 {
    x: int;
    y: int;
}

function int helper_59060(int a_58295, int b_23218) {
    let x_72525 = ((a_58295 | 1) - (a_58295 ^ a_58295));
    print_int(5);
    let x_43972 = b_23218;
    print_int(b_23218);
    let x_78599 = 255;
    a_58295 = (1 | b_23218);
    for i_86458 in 0..5 {
        i_86458 = ((1000 ^ x_78599) | (-42 * x_43972));
    }
    return (-10 % (x_78599 ^ -1));
}

function int main() {
    let res = helper_59060(10, 20);
    print_int(res);
    switch 0 {
        case 0: return 0;
        case 1: return 1;
        else: return 0;
    }
    print_int((3 / -5));
    print_int((2 / -10));
    return 0;
}
```

### Compiler Output:
```text
5
20
```
---

## Finding `993d741c3c40` — RUNTIME_HANG
- **Timestamp**: `2026-11-08 00:05:02Z`
- **Exit Code**: `-999` (`0xfffffc19`)
- **Reason**: Executable Fault during --run: Compiler Hang / Infinite Loop (>10s)
- **Target Mode**: `types`
- **Artifact File**: `fuzzer/crashes/crash_runtime_hang_993d741c3c40.sb`

### Minimal Reproducer:
```sable
// Auto-generated High-Validity Sable Module

view Header_31299 {
    magic: int @ 0;
    flags: int @ 8;
}

function int helper_18430(int a_60430, int b_24689) {
    for i_83298 in 1..10 {
        let x_81440 = 255;
    }
    a_60430 = 1;
    let x_75058 = a_60430;
    for i_97965 in 1..5 {
        i_97965 = (i_97965 % (10 ^ -10));
    }
    return ((a_60430 | x_75058) % (a_60430 / a_60430));
}

function int main() {
    let res = helper_18430(10, 20);
    print_int(res);
    print_int((10 | 1));
    let x_47766 = ((42 - 3) * (100 | 10));
    return 0;
}
```

### Compiler Output:
```text

```
---

## Finding `5a40cfb39e20` — RUNTIME_HANG
- **Timestamp**: `2026-11-08 00:05:40Z`
- **Exit Code**: `-999` (`0xfffffc19`)
- **Reason**: Executable Fault during --run: Compiler Hang / Infinite Loop (>10s)
- **Target Mode**: `types`
- **Artifact File**: `fuzzer/crashes/crash_runtime_hang_5a40cfb39e20.sb`

### Minimal Reproducer:
```sable
// Auto-generated High-Validity Sable Module

struct Point_96012 {
    x: int;
    y: int;
}

view Header_73699 {
    magic: int @ 0;
    flags: int @ 8;
}

function int helper_79221(int a_76406, int b_31052) {
    let x_24622 = 0;
    for i_83767 in 0..10 {
        for i_6349 in 1..10 {
            i_6349 = x_24622;
        }
    }
    if (-5 >= (x_24622 ^ b_31052)) {
        let x_54642 = 1;
    } else {
        let x_14820 = 2;
    }
    return -5;
}

function int main() {
    let res = helper_79221(10, 20);
    print_int(res);
    print_int(-10);
    let x_28227 = ((1 % 10) / -5);
    let x_9322 = ((x_28227 | x_28227) + x_28227);
    let x_12087 = (x_9322 % (x_9322 * x_9322));
    let x_86078 = ((x_28227 & x_9322) % (x_12087 % x_28227));
    return 0;
}
```

### Compiler Output:
```text

```
---

## Finding `9d5d4cfa96f7` — RUNTIME_HANG
- **Timestamp**: `2026-11-08 00:06:23Z`
- **Exit Code**: `-999` (`0xfffffc19`)
- **Reason**: Executable Fault during --run: Compiler Hang / Infinite Loop (>10s)
- **Target Mode**: `types`
- **Artifact File**: `fuzzer/crashes/crash_runtime_hang_9d5d4cfa96f7.sb`

### Minimal Reproducer:
```sable
// Auto-generated High-Validity Sable Module

function int helper_43706(int a_82866, int b_4182) {
    for i_35908 in 1..10 {
        for i_70182 in 1..5 {
            i_70182 = ((2 % a_82866) / (b_4182 | a_82866));
        }
    }
    let x_9760 = 42;
    let x_11681 = (x_9760 ^ (255 & -10));
    return 100;
}

function int main() {
    let res = helper_43706(10, 20);
    print_int(res);
    print_int((-1 & 1000));
    print_int((-5 & 5));
    return 0;
}
```

### Compiler Output:
```text

```
---

## Finding `e2a287f50bb7` — RUNTIME_HANG
- **Timestamp**: `2026-11-08 00:07:09Z`
- **Exit Code**: `-999` (`0xfffffc19`)
- **Reason**: Executable Fault during --run: Compiler Hang / Infinite Loop (>10s)
- **Target Mode**: `types`
- **Artifact File**: `fuzzer/crashes/crash_runtime_hang_e2a287f50bb7.sb`

### Minimal Reproducer:
```sable
// Auto-generated High-Validity Sable Module

struct Point_49232 {
    x: int;
    y: int;
}

function int helper_8175(int a_24266, int b_77468) {
    print_int(b_77468);
    let x_32497 = (a_24266 & (a_24266 / -10));
    print_int(-10);
    let x_55737 = x_32497;
    let x_27542 = x_32497;
    let x_65722 = ((5 + x_55737) + a_24266);
    let x_4021 = ((x_32497 & b_77468) & (-1 / 1));
    return 10;
}

function int main() {
    let res = helper_8175(10, 20);
    print_int(res);
    for i_88871 in 1..10 {
        for i_15811 in 1..10 {
            i_15811 = i_88871;
        }
    }
    for i_1446 in 1..5 {
        let x_68681 = ((-1 % 255) % (5 / 42));
    }
    for i_23300 in 0..10 {
        let x_17636 = 0;
    }
    let x_30019 = (i_1446 & (i_1446 * i_23300));
    return 0;
}
```

### Compiler Output:
```text
20
-10
10
```
---

## Finding `de792216447f` — RUNTIME_HANG
- **Timestamp**: `2026-11-08 00:07:42Z`
- **Exit Code**: `-999` (`0xfffffc19`)
- **Reason**: Executable Fault during --run: Compiler Hang / Infinite Loop (>10s)
- **Target Mode**: `types`
- **Artifact File**: `fuzzer/crashes/crash_runtime_hang_de792216447f.sb`

### Minimal Reproducer:
```sable
// Auto-generated High-Validity Sable Module

function int helper_63289(int a_92363, int b_33702) {
    for i_54646 in 0..10 {
        i_54646 = (-5 % a_92363);
    }
    let x_85965 = ((42 | b_33702) ^ (1000 + 42));
    let x_81764 = a_92363;
    let x_10751 = ((a_92363 % b_33702) % (5 % b_33702));
    return x_10751;
}

function int main() {
    let res = helper_63289(10, 20);
    print_int(res);
    let x_5910 = ((255 * 5) ^ (-42 / -1));
    let x_93291 = x_5910;
    print_int((-42 & x_93291));
    return 0;
}
```

### Compiler Output:
```text

```
---

## Finding `8d340aa6f73a` — RUNTIME_HANG
- **Timestamp**: `2026-11-08 00:08:39Z`
- **Exit Code**: `-999` (`0xfffffc19`)
- **Reason**: Executable Fault during --run: Compiler Hang / Infinite Loop (>10s)
- **Target Mode**: `types`
- **Artifact File**: `fuzzer/crashes/crash_runtime_hang_8d340aa6f73a.sb`

### Minimal Reproducer:
```sable
// Auto-generated High-Validity Sable Module

struct Point_30821 {
    x: int;
    y: int;
}

function int helper_89230(int a_30236, int b_64472) {
    print_int((0 / a_30236));
    let x_63829 = b_64472;
    let x_33638 = ((x_63829 + a_30236) ^ x_63829);
    switch x_63829 {
        case 0: return 0;
        case 1: return 1;
        else: return 0;
    }
    print_int((1000 ^ a_30236));
    let x_134 = x_33638;
    return ((5 | a_30236) % (a_30236 % b_64472));
}

function int main() {
    let res = helper_89230(10, 20);
    print_int(res);
    print_int((-42 ^ 3));
    for i_36934 in 0..10 {
        let x_91283 = i_36934;
    }
    for i_66806 in 1..10 {
        for i_90527 in 1..10 {
            let x_6310 = ((3 | 3) & (i_36934 - i_36934));
        }
    }
    for i_94942 in 1..10 {
        i_94942 = 2;
    }
    return 0;
}
```

### Compiler Output:
```text
0
0
-43
```
---

## Finding `b236be3bfde5` — RUNTIME_HANG
- **Timestamp**: `2026-11-08 00:08:58Z`
- **Exit Code**: `-999` (`0xfffffc19`)
- **Reason**: Executable Fault during --run: Compiler Hang / Infinite Loop (>10s)
- **Target Mode**: `types`
- **Artifact File**: `fuzzer/crashes/crash_runtime_hang_b236be3bfde5.sb`

### Minimal Reproducer:
```sable
// Auto-generated High-Validity Sable Module

function int helper_643(int a_79932, int b_20068) {
    for i_28923 in 0..10 {
        i_28923 = -5;
    }
    let x_4721 = 3;
    a_79932 = ((a_79932 ^ x_4721) - (b_20068 | x_4721));
    let x_57239 = (a_79932 + 1);
    let x_18090 = a_79932;
    return a_79932;
}

function int main() {
    let res = helper_643(10, 20);
    print_int(res);
    print_int((10 / 2));
    let x_99833 = (1000 + (-42 & 255));
    print_int((1 % x_99833));
    return 0;
}
```

### Compiler Output:
```text

```
---

## Finding `5f9c0a69f27c` — RUNTIME_HANG
- **Timestamp**: `2026-11-08 00:12:47Z`
- **Exit Code**: `-999` (`0xfffffc19`)
- **Reason**: Executable Fault during --run: Compiler Hang / Infinite Loop (>10s)
- **Target Mode**: `types`
- **Artifact File**: `fuzzer/crashes/crash_runtime_hang_5f9c0a69f27c.sb`

### Minimal Reproducer:
```sable
struct Point_72669 {
}
function int helper_28098(int a_69678, int b_70806) {
    switch b_70806 {
        case 0: return 0;
    }
}
function int main() {
    for i_67974 in 0..5 {
        for i_24614 in 1..10 {
            i_67974 = -5;
        }
    }
}
```

### Compiler Output:
```text
0
```
---
